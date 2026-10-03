// Prüft die Fix-Logik aus native/src/music_hook.cpp (tracked_rwops.h) auf der Quest gegen die echten
// Filmdateien: vorbisfile öffnet den Filmton mit denselben Rückrufen wie SDL_mixer (music_ogg.c,
// sdl_seek_func/sdl_tell_func über SDL_RWseek/SDL_RWtell)
//  - über eine Quelle wie Stratagus CFile (Spulen liefert 0 statt der Position) mit Positions-Hülle → schnell;
//  - mit „roh“ ohne Hülle → der bisherige Hänger (mit timeout aufrufen).
// SDL wird nicht initialisiert (auf Android ohne Java-Umgebung nicht möglich); RWops brauchen das nicht.
//
//   test_music_hook [roh] <film.ogv> ...
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <vorbis/vorbisfile.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

#include "tracked_rwops.h"

namespace {

int g_failures = 0;

// Nachbildung von Stratagus CFile::to_SDL_RWops (iolib.cpp): seek gibt das Ergebnis von fseek zurück.
Sint64 SDLCALL cfSize(SDL_RWops* rw) {
  FILE* f = static_cast<FILE*>(rw->hidden.unknown.data1);
  long cur = std::ftell(f);
  std::fseek(f, 0, SEEK_END);
  long size = std::ftell(f);
  std::fseek(f, cur, SEEK_SET);
  return size;
}
Sint64 SDLCALL cfSeek(SDL_RWops* rw, Sint64 off, int whence) {
  return std::fseek(static_cast<FILE*>(rw->hidden.unknown.data1), static_cast<long>(off), whence);
}
size_t SDLCALL cfRead(SDL_RWops* rw, void* p, size_t size, size_t n) {
  return std::fread(p, 1, size * n, static_cast<FILE*>(rw->hidden.unknown.data1)) / size;
}
size_t SDLCALL cfWrite(SDL_RWops*, const void*, size_t, size_t) { return 0; }
int SDLCALL cfClose(SDL_RWops* rw) {
  std::fclose(static_cast<FILE*>(rw->hidden.unknown.data1));
  SDL_FreeRW(rw);
  return 0;
}

SDL_RWops* cfileLike(const char* path) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return nullptr;
  SDL_RWops* rw = SDL_AllocRW();
  rw->size = cfSize;
  rw->seek = cfSeek;
  rw->read = cfRead;
  rw->write = cfWrite;
  rw->close = cfClose;
  rw->hidden.unknown.data1 = f;
  return rw;
}

// Rückrufe wie SDL_mixer src/codecs/music_ogg.c
size_t sdlRead(void* ptr, size_t size, size_t n, void* src) { return SDL_RWread(static_cast<SDL_RWops*>(src), ptr, size, n); }
int sdlSeek(void* src, ogg_int64_t off, int whence) {
  return SDL_RWseek(static_cast<SDL_RWops*>(src), off, whence) < 0 ? -1 : 0;
}
long sdlTell(void* src) { return static_cast<long>(SDL_RWtell(static_cast<SDL_RWops*>(src))); }

double since(std::chrono::steady_clock::time_point t) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
}

}  // namespace

int main(int argc, char** argv) {
  int first = 1;
  const bool raw = argc > 1 && std::strcmp(argv[1], "roh") == 0;
  if (raw) ++first;
  if (first >= argc) {
    std::fprintf(stderr, "Aufruf: %s [roh] <film.ogv> ...\n", argv[0]);
    return 2;
  }
  for (int i = first; i < argc; ++i) {
    SDL_RWops* src = cfileLike(argv[i]);
    if (!src) {
      std::printf("FEHLER: %s nicht lesbar\n", argv[i]);
      ++g_failures;
      continue;
    }
    SDL_RWops* rw = raw ? src : xrs::withTrackedPosition(src, true);
    // Positions-Hülle: tell nach Lesen/Spulen stimmt
    if (!raw) {
      char probe[100];
      SDL_RWread(rw, probe, 1, sizeof(probe));
      bool ok = SDL_RWtell(rw) == 100 && SDL_RWseek(rw, -10, RW_SEEK_END) == SDL_RWsize(rw) - 10 &&
                SDL_RWseek(rw, 0, RW_SEEK_SET) == 0;
      std::printf("%s: Position nach Lesen/Spulen %s\n", argv[i], ok ? "korrekt" : "FALSCH");
      if (!ok) ++g_failures;
    }
    ov_callbacks cb{sdlRead, sdlSeek, nullptr, sdlTell};
    OggVorbis_File vf;
    auto t0 = std::chrono::steady_clock::now();
    int rc = ov_open_callbacks(rw, &vf, nullptr, 0, cb);
    double dt = since(t0);
    if (rc < 0) {
      std::printf("%s: kein Vorbis-Ton (%d) nach %.3f s\n", argv[i], rc, dt);
    } else {
      double total = ov_time_total(&vf, -1);
      std::printf("%s: geöffnet in %.3f s, Ton %.1f s\n", argv[i], dt, total);
      if (dt > 1.0 || total <= 0) ++g_failures;
      ov_clear(&vf);
    }
    SDL_RWclose(rw);
  }
  std::printf("test_music_hook: %d Fehler\n", g_failures);
  return g_failures ? 1 : 0;
}
