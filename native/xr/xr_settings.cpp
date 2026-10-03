#include "xr_settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "xr_log.h"

namespace xrs {
namespace {

// Reglerwert auf 0–100 in 10er-Schritten bringen.
int slider(const char* value) { return std::clamp((std::atoi(value) + 5) / 10 * 10, 0, 100); }

}  // namespace

std::string settingsPath() {
  const char* dir = std::getenv("XRS_USER_DIR");  // nicht im Datenordner: den ersetzt ein Import
  return std::string(dir && *dir ? dir : ".") + "/xr-settings.txt";
}

Settings loadSettings() {
  Settings s;
  std::string path = settingsPath();
  FILE* f = std::fopen(path.c_str(), "r");
  if (!f) {
    XLOGI("settings", "keine gespeicherten VR-Einstellungen, Standardwerte");
    return s;
  }
  char line[256];
  while (std::fgets(line, sizeof(line), f)) {
    char* eq = std::strchr(line, '=');
    if (!eq) continue;
    *eq = '\0';
    const char* key = line;
    const char* value = eq + 1;
    if (!std::strcmp(key, "pointer_hand")) s.pointerHand = std::atoi(value) == 0 ? 0 : 1;
    else if (!std::strcmp(key, "width")) s.width = std::strtof(value, nullptr);
    else if (!std::strcmp(key, "distance")) s.distance = std::strtof(value, nullptr);
    else if (!std::strcmp(key, "beam")) s.beam = std::atoi(value) != 0;
    else if (!std::strcmp(key, "target")) s.target = static_cast<TargetStyle>(std::atoi(value) % 3);
    else if (!std::strcmp(key, "log_input")) s.logInput = std::atoi(value) != 0;
    else if (!std::strcmp(key, "passthrough")) s.passthrough = std::atoi(value) != 0;
    else if (!std::strcmp(key, "auto_skip_movies")) s.autoSkipMovies = std::atoi(value) != 0;
    else if (!std::strcmp(key, "menu_orc")) s.menuOrc = std::atoi(value) != 0;
    else if (!std::strcmp(key, "game_cursor")) s.gameCursor = std::atoi(value) != 0;
    else if (!std::strcmp(key, "brightness")) s.image.brightness = slider(value);
    else if (!std::strcmp(key, "contrast")) s.image.contrast = slider(value);
    else if (!std::strcmp(key, "saturation")) s.image.saturation = slider(value);
    else if (!std::strcmp(key, "sharpness")) s.image.sharpness = slider(value);
    else if (!std::strcmp(key, "crt")) s.image.crt = std::clamp(std::atoi(value), 0, 2);
  }
  std::fclose(f);
  XLOGI("settings",
        "geladen: Hand=%s Breite=%.2f Abstand=%.2f Strahl=%d Ziel=%d Eingabelog=%d Passthrough=%d "
        "Bild H%d K%d S%d Sch%d Modus%d",
        s.pointerHand ? "rechts" : "links", s.width, s.distance, s.beam, static_cast<int>(s.target), s.logInput,
        s.passthrough, s.image.brightness, s.image.contrast, s.image.saturation, s.image.sharpness, s.image.crt);
  return s;
}

void saveSettings(const Settings& s) {
  std::string path = settingsPath();
  std::string tmp = path + ".tmp";
  FILE* f = std::fopen(tmp.c_str(), "w");
  if (!f) {
    XLOGW("settings", "nicht speicherbar: %s", tmp.c_str());
    return;
  }
  std::fprintf(f,
               "pointer_hand=%d\nwidth=%.3f\ndistance=%.3f\nbeam=%d\ntarget=%d\nlog_input=%d\npassthrough=%d\n"
               "brightness=%d\ncontrast=%d\nsaturation=%d\nsharpness=%d\nauto_skip_movies=%d\nmenu_orc=%d\n"
               "game_cursor=%d\ncrt=%d\n",
               s.pointerHand, s.width, s.distance, s.beam ? 1 : 0, static_cast<int>(s.target), s.logInput ? 1 : 0,
               s.passthrough ? 1 : 0, s.image.brightness, s.image.contrast, s.image.saturation, s.image.sharpness,
               s.autoSkipMovies ? 1 : 0, s.menuOrc ? 1 : 0, s.gameCursor ? 1 : 0, s.image.crt);
  std::fclose(f);
  std::rename(tmp.c_str(), path.c_str());
  XLOGD("settings", "gespeichert");
}

}  // namespace xrs
