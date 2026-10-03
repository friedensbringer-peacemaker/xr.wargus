// SDL-Datenquelle mit selbst geführter Position: Hülle um eine Quelle, deren Spulfunktion nicht die neue
// Position zurückgibt (Stratagus CFile::to_SDL_RWops liefert das Ergebnis von fseek, also 0). Annahme: die
// innere Quelle ist frisch geöffnet (Position 0). Genutzt von music_hook.cpp, geprüft von
// tests/test_music_hook.cpp.
#pragma once

#include <SDL.h>

namespace xrs {

struct Tracked {
  SDL_RWops* inner;
  bool freeInner;
  Sint64 pos;
};

inline Tracked* tracked(SDL_RWops* rw) { return static_cast<Tracked*>(rw->hidden.unknown.data1); }

inline Sint64 SDLCALL trackedSize(SDL_RWops* rw) { return SDL_RWsize(tracked(rw)->inner); }

inline Sint64 SDLCALL trackedSeek(SDL_RWops* rw, Sint64 offset, int whence) {
  Tracked* t = tracked(rw);
  Sint64 target = offset;
  if (whence == RW_SEEK_CUR) {
    target = t->pos + offset;
  } else if (whence == RW_SEEK_END) {
    Sint64 size = SDL_RWsize(t->inner);
    if (size < 0) return -1;
    target = size + offset;
  }
  if (target < 0) return SDL_SetError("Spulen vor den Dateianfang");
  if (target != t->pos && SDL_RWseek(t->inner, target, RW_SEEK_SET) < 0) return -1;
  t->pos = target;
  return target;
}

inline size_t SDLCALL trackedRead(SDL_RWops* rw, void* ptr, size_t size, size_t count) {
  Tracked* t = tracked(rw);
  size_t n = SDL_RWread(t->inner, ptr, size, count);
  t->pos += static_cast<Sint64>(n * size);
  return n;
}

inline size_t SDLCALL trackedWrite(SDL_RWops*, const void*, size_t, size_t) { return 0; }

inline int SDLCALL trackedClose(SDL_RWops* rw) {
  Tracked* t = tracked(rw);
  int rc = t->freeInner ? SDL_RWclose(t->inner) : 0;
  delete t;
  SDL_FreeRW(rw);
  return rc;
}

inline SDL_RWops* withTrackedPosition(SDL_RWops* inner, bool freeInner) {
  SDL_RWops* rw = SDL_AllocRW();
  if (!rw) return nullptr;
  rw->size = trackedSize;
  rw->seek = trackedSeek;
  rw->read = trackedRead;
  rw->write = trackedWrite;
  rw->close = trackedClose;
  rw->type = SDL_RWOPS_UNKNOWN;
  rw->hidden.unknown.data1 = new Tracked{inner, freeInner, 0};
  return rw;
}


}  // namespace xrs
