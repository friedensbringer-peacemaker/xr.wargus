#include "xr_watchdog.h"

#include <dirent.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include "xr_log.h"

namespace xrs {
namespace {

constexpr int kMaxFrames = 40;
// Stapel bei 3 s, 10 s und 30 s Stillstand: zeigt, ob der Thread an einer Stelle steht oder läuft.
constexpr double kDumpAfter[] = {3.0, 10.0, 30.0};
constexpr int kDumps = static_cast<int>(sizeof(kDumpAfter) / sizeof(kDumpAfter[0]));

std::atomic<pid_t> g_mainTid{0};
uintptr_t g_frames[kMaxFrames];
std::atomic<int> g_count{0};
std::atomic<bool> g_ready{false};
bool g_installed = false;

uint64_t g_stallFrom = 0;  // lastPresentNs des laufenden Stillstands
int g_dumpsDone = 0;

// Liest 16 Byte (Rahmenzeiger + Rücksprungadresse) per Systemaufruf: ungültige Adressen liefern
// einen Fehler statt eines Absturzes im Signalhandler.
bool readPair(uintptr_t addr, uintptr_t out[2]) {
  iovec local{out, 2 * sizeof(uintptr_t)};
  iovec remote{reinterpret_cast<void*>(addr), 2 * sizeof(uintptr_t)};
  return syscall(SYS_process_vm_readv, getpid(), &local, 1, &remote, 1, 0) ==
         static_cast<long>(2 * sizeof(uintptr_t));
}

void onSignal(int, siginfo_t*, void* context) {
  int n = 0;
#if defined(__aarch64__)
  const auto* uc = static_cast<const ucontext_t*>(context);
  uintptr_t pc = uc->uc_mcontext.pc;
  uintptr_t lr = uc->uc_mcontext.regs[30];
  uintptr_t fp = uc->uc_mcontext.regs[29];
  g_frames[n++] = pc;
  g_frames[n++] = lr;
  // Rahmenzeiger-Kette (arm64-Android baut mit Rahmenzeigern): [fp] = vorheriger fp, [fp+8] = lr.
  while (n < kMaxFrames && fp && (fp & 0xf) == 0) {
    uintptr_t pair[2];
    if (!readPair(fp, pair)) break;
    if (pair[1]) g_frames[n++] = pair[1] & 0x00ffffffffffffffull;  // Tag-Bits (MTE/PAC) entfernen
    if (pair[0] <= fp) break;
    fp = pair[0];
  }
#else
  (void)context;
#endif
  g_count.store(n);
  g_ready.store(true);
}

void install() {
  if (g_installed) return;
  g_installed = true;
  struct sigaction sa{};
  sa.sa_sigaction = onSignal;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR2, &sa, nullptr);
}

std::string readSmall(const std::string& path) {
  std::string out;
  if (FILE* f = std::fopen(path.c_str(), "r")) {
    char buf[512];
    size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    out = buf;
    std::fclose(f);
  }
  while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
  return out;
}

// Zustand eines Threads aus /proc/self/task/<tid>/stat: Zustand (R/S/D) und CPU-Zeit in Ticks.
std::string threadState(const std::string& tid) {
  std::string stat = readSmall("/proc/self/task/" + tid + "/stat");
  size_t close = stat.rfind(')');
  if (close == std::string::npos || close + 2 >= stat.size()) return "?";
  char state = stat[close + 2];
  unsigned long utime = 0, stime = 0;
  // Felder nach dem Zustand: ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime
  std::sscanf(stat.c_str() + close + 3, "%*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &utime, &stime);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%c cpu %lu", state, utime + stime);
  return buf;
}

void logThreads() {
  DIR* d = opendir("/proc/self/task");
  if (!d) return;
  while (dirent* e = readdir(d)) {
    if (e->d_name[0] == '.') continue;
    std::string tid = e->d_name;
    std::string name = readSmall("/proc/self/task/" + tid + "/comm");
    std::string wchan = readSmall("/proc/self/task/" + tid + "/wchan");
    XLOGI("watchdog", "  Thread %s %-16s %s %s", tid.c_str(), name.c_str(), threadState(tid).c_str(),
          wchan.c_str());
  }
  closedir(d);
}

void dump(double stalled, const char* what) {
  pid_t tid = g_mainTid.load();
  XLOGW("watchdog", "Engine-Thread liefert seit %.1f s kein Bild (%s) – Thread %d: %s", stalled, what,
        static_cast<int>(tid), threadState(std::to_string(tid)).c_str());
  if (g_dumpsDone == 0) logThreads();
  if (!tid) return;
  g_ready.store(false);
  if (syscall(SYS_tgkill, getpid(), tid, SIGUSR2) != 0) return;
  for (int i = 0; i < 200 && !g_ready.load(); ++i) {
    timespec ts{0, 1000000};
    nanosleep(&ts, nullptr);
  }
  if (!g_ready.load()) {
    XLOGW("watchdog", "  kein Stapel (Signal nicht zugestellt)");
    return;
  }
  int n = g_count.load();
  for (int i = 0; i < n; ++i) {
    Dl_info info{};
    uintptr_t pc = g_frames[i];
    // Rücksprungadressen zeigen hinter den Aufruf: für die Symbolsuche 4 Byte zurück.
    uintptr_t lookup = i > 0 && pc >= 4 ? pc - 4 : pc;
    if (dladdr(reinterpret_cast<void*>(lookup), &info) && info.dli_fname) {
      const char* lib = std::strrchr(info.dli_fname, '/');
      lib = lib ? lib + 1 : info.dli_fname;
      uintptr_t off = lookup - reinterpret_cast<uintptr_t>(info.dli_fbase);
      if (info.dli_sname) {
        XLOGI("watchdog", "  #%02d %s+0x%zx %s+0x%zx", i, lib, static_cast<size_t>(off), info.dli_sname,
              static_cast<size_t>(lookup - reinterpret_cast<uintptr_t>(info.dli_saddr)));
      } else {
        XLOGI("watchdog", "  #%02d %s+0x%zx", i, lib, static_cast<size_t>(off));
      }
    } else {
      XLOGI("watchdog", "  #%02d 0x%zx", i, static_cast<size_t>(pc));
    }
  }
}

}  // namespace

void watchdogMainThread() {
  if (g_mainTid.load()) return;
  install();
  g_mainTid.store(static_cast<pid_t>(syscall(SYS_gettid)));
}

void watchdogPoll(uint64_t nowNs, uint64_t lastPresentNs, const char* what) {
  if (!lastPresentNs || nowNs < lastPresentNs) return;
  if (lastPresentNs != g_stallFrom) {
    if (g_dumpsDone > 0) XLOGI("watchdog", "Engine-Thread liefert wieder Bilder");
    g_stallFrom = lastPresentNs;
    g_dumpsDone = 0;
  }
  if (g_dumpsDone >= kDumps) return;
  double stalled = static_cast<double>(nowNs - lastPresentNs) * 1e-9;
  if (stalled < kDumpAfter[g_dumpsDone]) return;
  dump(stalled, what);
  ++g_dumpsDone;
}

}  // namespace xrs
