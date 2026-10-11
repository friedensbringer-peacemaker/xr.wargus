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
  // Tisch-Ansicht (Werte und Grenzen wie xr-common / XR-Settlers): Brett vor dem Kopf, gekippt
  int view = 0;              // 0 = Leinwand, 1 = Tisch
  float tableWidth = 1.4f;
  float tableDistance = 1.0f;
  float tableTilt = 45.0f;   // Grad aus der Senkrechten (30–75)
  float tableLower = 40.0f;  // Tischmitte in cm unter Blickhöhe (0–90)
  // Karte bewegen: Stick-Scrolltempo 1–10, Kartenziehen mit dem Griff (0 = 1×, 1 = 1,5×, 2 = 2×, 3 = 3×)
  int scrollSpeed = 5;
  int dragSpeed = 0;
  bool beam = true;
  TargetStyle target = TargetStyle::kRing;
  bool logInput = false;
  bool passthrough = false;
  bool autoSkipMovies = false;  // Filme (Intro, Zwischensequenzen) sofort überspringen
  bool menuOrc = false;         // VR-Menü im Ork- statt Menschenstil
  float pointerOffsetX = 0.0f;  // Zeiger-Versatz in % der Bildbreite (+ = rechts), siehe ptroff
  float pointerOffsetY = 0.0f;  // in % der Bildhöhe (+ = unten)
  bool gameCursor = true;       // spieleigener Mauszeiger (P3 „Spiel-Mauszeiger: Aus“)
  ImageSettings image;
};

std::string settingsPath();
Settings loadSettings();
void saveSettings(const Settings& s);

}  // namespace xrs
