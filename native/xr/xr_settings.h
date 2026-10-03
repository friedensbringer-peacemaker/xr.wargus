// Persistente VR-Einstellungen (files/xr-settings.txt, Zeilen "schluessel=wert").
// Gespeichert werden Breite und Abstand, nicht Position/Richtung: gestartet wird
// immer zentriert vor der Blickrichtung.
#pragma once

#include <string>

namespace xrs {

enum class TargetStyle { kOff = 0, kRing = 1, kDot = 2 };

// Bildregler 0–100 in 10er-Schritten; 50 = unverändert (Schärfe: 0 = aus).
struct ImageSettings {
  int brightness = 50;
  int contrast = 50;
  int saturation = 50;
  int sharpness = 0;
  int crt = 0;  // Bildmodus: 0 = normal, 1 = Scanlines, 2 = CRT (Scanlines + Maske + Vignette)
  bool neutral() const { return brightness == 50 && contrast == 50 && saturation == 50 && sharpness == 0 && crt == 0; }
};

struct Settings {
  int pointerHand = 1;  // 0 = links, 1 = rechts
  float width = 2.0f;
  float distance = 1.8f;
  bool beam = true;
  TargetStyle target = TargetStyle::kRing;
  bool logInput = false;
  bool passthrough = false;
  bool autoSkipMovies = false;  // Filme (Intro, Zwischensequenzen) sofort überspringen
  bool menuOrc = false;         // VR-Menü im Ork- statt Menschenstil
  bool gameCursor = true;       // spieleigener Mauszeiger (P3 „Spiel-Mauszeiger: Aus“)
  ImageSettings image;
};

std::string settingsPath();
Settings loadSettings();
void saveSettings(const Settings& s);

}  // namespace xrs
