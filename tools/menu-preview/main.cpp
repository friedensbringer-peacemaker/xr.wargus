// Desktop-Vorschau der VR-Anzeigen (ohne Headset): zeichnet VR-Menü (alle Reiter, Mensch/Ork),
// VR-Tastatur und Ladeanzeige mit genau dem Code aus native/xr und speichert PNGs.
//
//   menu-preview <data.Wargus-Ordner> <Ausgabeordner> [Schrift.ttf]
//
// Build: tools/menu-preview/build.sh (llvm-mingw, Host-SDL2 aus .build/host-stratagus).

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "xr_keyboard.h"
#include "xr_loader.h"
#include "xr_log.h"
#include "xr_menu.h"
#include "xr_settings.h"
#include "xr_theme.h"

// xr_log.cpp ist Android-spezifisch: hier schlicht auf stderr.
namespace xrs {
void logf(LogLevel, const char* area, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::fprintf(stderr, "%s: ", area);
  std::vfprintf(stderr, fmt, args);
  std::fputc('\n', stderr);
  va_end(args);
}
}  // namespace xrs

namespace {

void setEnv(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void save(const uint8_t* pixels, int w, int h, const std::string& file) {
  SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uint8_t*>(pixels), w, h, 32, w * 4,
                                                      SDL_PIXELFORMAT_RGBA32);
  if (!s || IMG_SavePNG(s, file.c_str()) != 0) {
    std::fprintf(stderr, "nicht speicherbar: %s (%s)\n", file.c_str(), SDL_GetError());
  } else {
    std::printf("%s\n", file.c_str());
  }
  SDL_FreeSurface(s);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "Aufruf: %s <data.Wargus> <Ausgabeordner> [Schrift.ttf]\n", argv[0]);
    return 1;
  }
  setEnv("XRS_DATA_DIR", argv[1]);
  if (argc > 3) setEnv("XRS_FONT", argv[3]);
  const std::string out = argv[2];
  SDL_SetMainReady();
  IMG_Init(IMG_INIT_PNG);

  xrs::Settings settings;
  settings.passthrough = true;
  for (int orc = 0; orc < 2; ++orc) {
    xrs::Theme::get().setOrc(orc != 0);
    settings.menuOrc = orc != 0;
    for (int page = xrs::kPageQuick; page <= xrs::kPageDisplay; ++page) {
      xrs::VrMenu menu;
      menu.setPage(page);
      menu.update(settings, xrs::GroupMode::kSelect, "Spiel 30 fps - XR 72 Hz - Fenster 2.0 m, 1.8 m entfernt", true,
                  page == xrs::kPageGame, false);
      menu.setHover(menu.navigate(-1, 0, 0));  // Startfokus wie bei Stick-Bedienung
      save(menu.pixels(), xrs::VrMenu::kWidth, xrs::VrMenu::kHeight,
           out + "/menu-" + (orc ? "ork" : "mensch") + "-" + std::to_string(page) + ".png");
    }
    // Beenden nach dem ersten Klick: sichtbar scharf (Pflichtumfang P3)
    xrs::VrMenu armed;
    armed.setPage(xrs::kPageQuick);
    armed.update(settings, xrs::GroupMode::kSelect, "Spiel 30 fps - XR 72 Hz - Fenster 2.0 m, 1.8 m entfernt", true,
                 false, true);
    save(armed.pixels(), xrs::VrMenu::kWidth, xrs::VrMenu::kHeight,
         out + "/menu-" + (orc ? "ork" : "mensch") + "-0-beenden-scharf.png");
  }
  xrs::Theme::get().setOrc(false);
  xrs::KeyboardPanel keyboard;
  keyboard.setHover(14);
  save(keyboard.pixels(), xrs::KeyboardPanel::kWidth, xrs::KeyboardPanel::kHeight, out + "/tastatur.png");

  xrs::LoaderPanel loader;
  xrs::LoaderState state;
  state.step = xrs::LoadStep::kEngine;
  state.seconds = 3.0f;
  state.stepSeconds = 1.0f;
  save(loader.pixels(state), xrs::LoaderPanel::kWidth, xrs::LoaderPanel::kHeight, out + "/ladeanzeige.png");
  xrs::LoaderPanel missing;
  state.dataMissing = true;
  state.logPath = "/storage/emulated/0/Android/data/xr.wargus2/files/logs/xr-20260929-220000.log";
  save(missing.pixels(state), xrs::LoaderPanel::kWidth, xrs::LoaderPanel::kHeight, out + "/daten-fehlen.png");
  return 0;
}
