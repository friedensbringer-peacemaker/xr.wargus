// Musik/Filmton ohne Hänger. Stratagus (sound_server.cpp, LoadMusic) versucht erst Mix_LoadMUS(name) und
// fällt sonst auf Mix_LoadMUS_RW(CFile::to_SDL_RWops(...)) zurück. Dessen Spulfunktion gibt den
// Rückgabewert von fseek zurück (0) statt der neuen Position – SDL_RWtell meldet so immer 0. vorbisfile
// sucht beim Öffnen vom Dateiende rückwärts nach der letzten Tonseite (_open_seekable2 →
// _get_prev_page_serial) und kommt mit der falschen Position nie zum Ende: die Engine hing beim Start im
// ersten Film mit 100 % CPU (Hänger-Wächter, Log vom 03.10.2026; am Gerät nachgestellt mit
// tests/test_movie_audio.cpp „broken“). Auf dem PC fällt das nicht auf, weil Mix_LoadMUS dort klappt;
// auf Android öffnet SDL_RWFromFile relative Pfade nur im internen Speicher bzw. in den Assets.
//
// Abhilfe ohne Upstream-Änderung, per Linker-Wrap der Aufrufe aus Stratagus:
//  - Mix_LoadMUS: relative Pfade auf das Arbeitsverzeichnis (= Datenordner) beziehen → SDL öffnet die
//    Datei per stdio mit korrekter Position.
//  - Mix_LoadMUS_RW: Datenquelle mit selbst geführter Position umhüllen (frisch geöffnete Datei, Start 0).
#include <SDL.h>
#include <SDL_mixer.h>
#include <unistd.h>

#include <string>

#include "tracked_rwops.h"

extern "C" {
Mix_Music* __real_Mix_LoadMUS(const char* file);
Mix_Music* __real_Mix_LoadMUS_RW(SDL_RWops* src, int freesrc);
}

extern "C" {

Mix_Music* __wrap_Mix_LoadMUS(const char* file) {
  if (file && *file && *file != '/') {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
      const std::string path = std::string(cwd) + "/" + file;
      return __real_Mix_LoadMUS(path.c_str());
    }
  }
  return __real_Mix_LoadMUS(file);
}

Mix_Music* __wrap_Mix_LoadMUS_RW(SDL_RWops* src, int freesrc) {
  if (!src) return __real_Mix_LoadMUS_RW(src, freesrc);
  SDL_RWops* rw = xrs::withTrackedPosition(src, freesrc != 0);
  if (!rw) return __real_Mix_LoadMUS_RW(src, freesrc);
  return __real_Mix_LoadMUS_RW(rw, 1);
}

}  // extern "C"
