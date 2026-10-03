// Linker-Wraps (-Wl,--wrap=…), damit Stratagus unverändert bleibt:
//   SDL_RenderPresent  → Frame an die XR-Darstellung (xr_app.cpp); läuft sie, entfällt das
//                        normale Present auf die unsichtbare Android-Fläche. Gilt auch für Filme
//                        (movie.cpp hat ein eigenes Present).
//   SDL_SetCursor      → Startsignal für die Ladeanzeige: die Oberfläche steht.
//   SDL_CreateTexture / SDL_DestroyTexture → Filmbeginn/-ende (movie.cpp legt dafür eine
//                        YV12-Textur an) – für „Film überspringen“ und die Ladeanzeige.

#include <SDL.h>

#include <atomic>

#include "xr_app.h"

namespace {
std::atomic<SDL_Texture*> g_movieTexture{nullptr};
}

extern "C" {

void __real_SDL_RenderPresent(SDL_Renderer* renderer);
void __real_SDL_SetCursor(SDL_Cursor* cursor);
SDL_Texture* __real_SDL_CreateTexture(SDL_Renderer* renderer, Uint32 format, int access, int w, int h);
void __real_SDL_DestroyTexture(SDL_Texture* texture);

void __wrap_SDL_RenderPresent(SDL_Renderer* renderer) {
  SDL_Window* window = SDL_RenderGetWindow(renderer);
  // Nur Frames des Fensters, nicht solche in eine Zieltextur.
  if (window && SDL_GetRenderTarget(renderer) == nullptr && xrs::presentFrame(renderer, window)) {
    return;
  }
  __real_SDL_RenderPresent(renderer);
}

void __wrap_SDL_SetCursor(SDL_Cursor* cursor) {
  xrs::onEngineCursor();
  __real_SDL_SetCursor(cursor);
}

SDL_Texture* __wrap_SDL_CreateTexture(SDL_Renderer* renderer, Uint32 format, int access, int w, int h) {
  SDL_Texture* texture = __real_SDL_CreateTexture(renderer, format, access, w, h);
  if (texture && format == SDL_PIXELFORMAT_YV12) {
    g_movieTexture.store(texture);
    xrs::onMovieStart();
  }
  return texture;
}

void __wrap_SDL_DestroyTexture(SDL_Texture* texture) {
  SDL_Texture* expected = texture;
  if (texture && g_movieTexture.compare_exchange_strong(expected, nullptr)) xrs::onMovieEnd();
  __real_SDL_DestroyTexture(texture);
}

}  // extern "C"
