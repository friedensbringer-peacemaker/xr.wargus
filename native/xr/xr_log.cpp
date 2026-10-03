#include "xr_log.h"

#include "xr_app.h"

#include <android/log.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace xrs {
namespace {

constexpr size_t kKeepFiles = 10;

std::mutex g_mutex;
FILE* g_file = nullptr;
bool g_opened = false;
// Einfaches Zeichenfeld statt std::string: wird ohne Konstruktor initialisiert, damit
// statische Konstruktoren anderer Dateien (xr_startup.cpp) schon loggen können.
char g_path[512] = {};
std::atomic<bool> g_inputLogging{false};
timespec g_start{};

void pruneOldLogs(const std::string& dir) {
  DIR* d = opendir(dir.c_str());
  if (!d) return;
  std::vector<std::string> names;
  while (dirent* e = readdir(d)) {
    std::string name = e->d_name;
    if (name.rfind("xr-", 0) == 0 && name.size() > 4 && name.substr(name.size() - 4) == ".log") {
      names.push_back(name);
    }
  }
  closedir(d);
  std::sort(names.begin(), names.end());
  // Die neue Datei kommt gleich dazu: Platz für sie lassen.
  while (names.size() >= kKeepFiles) {
    std::remove((dir + "/" + names.front()).c_str());
    names.erase(names.begin());
  }
  // Logs aus 0.2.2 gehören der App-Gruppe (Setgid fehlte) und sind für adb unlesbar: neu
  // schreiben, damit sie die Gruppe des Ordners erben.
  struct stat dirStat{};
  if (stat(dir.c_str(), &dirStat) != 0) return;
  for (const std::string& name : names) {
    std::string path = dir + "/" + name;
    struct stat st{};
    if (stat(path.c_str(), &st) != 0 || st.st_gid == dirStat.st_gid) continue;
    std::string tmp = path + ".tmp";
    FILE* in = std::fopen(path.c_str(), "rb");
    FILE* out = in ? std::fopen(tmp.c_str(), "wb") : nullptr;
    if (in && out) {
      char buf[8192];
      size_t n;
      while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) std::fwrite(buf, 1, n, out);
    }
    if (in) std::fclose(in);
    if (out) {
      std::fclose(out);
      chmod(tmp.c_str(), 0660);
      std::rename(tmp.c_str(), path.c_str());
    }
  }
}

bool copyFile(const std::string& from, const std::string& to) {
  FILE* in = std::fopen(from.c_str(), "rb");
  if (!in) return false;
  FILE* out = std::fopen(to.c_str(), "wb");
  if (out) {
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) std::fwrite(buf, 1, n, out);
    std::fclose(out);
  }
  std::fclose(in);
  return out != nullptr;
}

// Der Log-Ordner muss das Setgid-Bit des Elternordners tragen, sonst gehören neue Dateien der
// App-Gruppe und adb (Gruppe ext_data_rw) kann sie nicht lesen. Die App darf das Bit nicht selbst
// setzen: chmod durch ein Nicht-Gruppenmitglied löscht es (so in 0.2.2/0.2.3 geschehen). Reparatur:
// Ordner beiseitelegen, neu anlegen (erbt Setgid), alte Logs hineinkopieren (erben die Gruppe).
void repairLogDir(const std::string& dir) {
  struct stat st{};
  if (stat(dir.c_str(), &st) != 0 || (st.st_mode & S_ISGID)) return;
  const std::string old = dir + ".alt";
  if (std::rename(dir.c_str(), old.c_str()) != 0) return;
  mkdir(dir.c_str(), 0770);
  if (DIR* d = opendir(old.c_str())) {
    while (dirent* e = readdir(d)) {
      std::string name = e->d_name;
      if (name == "." || name == "..") continue;
      copyFile(old + "/" + name, dir + "/" + name);
      std::remove((old + "/" + name).c_str());
    }
    closedir(d);
  }
  rmdir(old.c_str());
}

// Unter dem Mutex aufrufen.
void openOnce() {
  if (g_opened) return;
  g_opened = true;
  clock_gettime(CLOCK_MONOTONIC, &g_start);
  const char* dir = std::getenv("XRS_LOG_DIR");
  if (!dir || !*dir) return;
  // Gruppe lesbar: im app-eigenen externen Speicher kann adb (Gruppe ext_data_rw) die Logs
  // dann abholen (scripts/quest.sh logs); run-as sieht diesen Speicher nicht. Kein chmod auf den
  // Ordner: mkdir erbt Setgid vom Elternordner, chmod würde es löschen (siehe repairLogDir).
  repairLogDir(dir);
  mkdir(dir, 0770);
  pruneOldLogs(dir);
  time_t now = time(nullptr);
  tm local{};
  localtime_r(&now, &local);
  char name[64];
  strftime(name, sizeof(name), "xr-%Y%m%d-%H%M%S.log", &local);
  std::snprintf(g_path, sizeof(g_path), "%s/%s", dir, name);
  g_file = std::fopen(g_path, "w");
  if (g_file) chmod(g_path, 0660);
  if (!g_file) {
    __android_log_print(ANDROID_LOG_WARN, "xr-stratagus", "Logdatei nicht anlegbar: %s", g_path);
    g_path[0] = 0;
  }
}

// Unter dem Mutex aufrufen.
void writeFileLine(char level, const char* area, const char* text) {
  if (!g_file) return;
  timespec mono{};
  clock_gettime(CLOCK_MONOTONIC, &mono);
  double since = static_cast<double>(mono.tv_sec - g_start.tv_sec) +
                 static_cast<double>(mono.tv_nsec - g_start.tv_nsec) * 1e-9;
  timespec wall{};
  clock_gettime(CLOCK_REALTIME, &wall);
  tm local{};
  localtime_r(&wall.tv_sec, &local);
  std::fprintf(g_file, "%02d:%02d:%02d.%03ld +%.3f [%c] %s: %s\n", local.tm_hour, local.tm_min,
               local.tm_sec, wall.tv_nsec / 1000000L, since, level, area, text);
  std::fflush(g_file);
}

}  // namespace

void logf(LogLevel level, const char* area, const char* fmt, ...) {
  char text[1024];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);

  int prio = ANDROID_LOG_INFO;
  char letter = 'I';
  switch (level) {
    case LogLevel::kDebug: prio = ANDROID_LOG_DEBUG; letter = 'D'; break;
    case LogLevel::kInfo: prio = ANDROID_LOG_INFO; letter = 'I'; break;
    case LogLevel::kWarn: prio = ANDROID_LOG_WARN; letter = 'W'; break;
    case LogLevel::kError: prio = ANDROID_LOG_ERROR; letter = 'E'; break;
  }
  __android_log_print(prio, "xr-stratagus", "%s: %s", area, text);
  std::lock_guard<std::mutex> lock(g_mutex);
  openOnce();
  writeFileLine(letter, area, text);
}

void logEngineLine(const char* line) {
  __android_log_write(ANDROID_LOG_INFO, "Stratagus", line);
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    openOnce();
    writeFileLine('I', "engine", line);
  }
  onEngineLine(line);  // Fehlererkennung für die Ladeanzeige
}

bool inputLogging() { return g_inputLogging.load(); }
void setInputLogging(bool enabled) { g_inputLogging.store(enabled); }

const char* logFilePath() {
  std::lock_guard<std::mutex> lock(g_mutex);
  openOnce();
  return g_path;
}

}  // namespace xrs
