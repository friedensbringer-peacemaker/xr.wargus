// Ladeanzeige in VR: steht an der Stelle des Spielfensters, bis die Engine ihr erstes
// echtes Bild (Intro-Film oder Hauptmenü) zeigt. Schritte, Fortschrittsbalken, Laufzeit,
// Bitte um Geduld – und bei einem Startfehler die Meldung der Engine statt einer Endlos-Warte.
#pragma once

#include <cstdint>
#include <string>

#include "xr_canvas.h"

namespace xrs {

enum class LoadStep { kVr = 0, kData = 1, kEngine = 2, kGame = 3, kDone = 4 };

struct LoaderState {
  LoadStep step = LoadStep::kVr;
  float seconds = 0.0f;         // seit Start der VR-Darstellung
  float stepSeconds = 0.0f;     // seit Beginn des aktuellen Schritts
  bool dataMissing = false;     // Originaldaten nicht gefunden
  bool error = false;           // Engine hat einen Fehler gemeldet
  std::string errorText;        // erste Fehlerzeile der Engine
  std::string logPath;
};

class LoaderPanel {
 public:
  static constexpr int kWidth = 1024;
  static constexpr int kHeight = 768;

  LoaderPanel() : canvas_(kWidth, kHeight) {}
  // Zeichnet neu, wenn sich die sichtbare Anzeige geändert hat (höchstens ~15× pro Sekunde).
  const uint8_t* pixels(const LoaderState& state);
  uint32_t version() const { return version_; }
  float progress() const { return shownProgress_; }

 private:
  void render(const LoaderState& state, float progress);

  Canvas canvas_;
  uint32_t version_ = 0;
  int lastTick_ = -1;
  float shownProgress_ = 0.0f;
  std::string lastKey_;
};

}  // namespace xrs
