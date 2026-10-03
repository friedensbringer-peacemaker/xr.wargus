// Logik des VR-Menü-Pflichtumfangs (Skill xr-vr-menu, Abschnitt 0, P3/P4): Beenden mit sichtbarer
// Bestätigung und Sichtbarkeit von Strahl/Zielpunkt/Spiel-Mauszeiger. Nur Logik, kein Zeichnen.
//
// TODO xrc/menu.h: inhaltsgleich mit <XR-Ordner>/_shared/xr-common/include/xrc/menu.h
// (xrc::ConfirmAction, xrc::PointerVisibility). Sobald xr-common getaggt ist, diesen Header durch
// das Submodul ersetzen (Typ-Aliase unten genügen dann). Für XR-0AD ebenso übertragbar.
#pragma once

#include <cmath>

namespace xrs {

struct ConfirmParams {
  float timeout = 3.0f;  // Sekunden, die der Knopf nach dem ersten Klick scharf bleibt
};

// Zwei-Klick-Bestätigung für gefährliche Aktionen („Beenden“). Erster Klick schaltet scharf (Knopf rot,
// „Sicher? Nochmal klicken“, Doppelpuls Haptik), zweiter Klick innerhalb timeout bestätigt. Ohne zweiten
// Klick fällt der Knopf zurück; update() meldet das, damit neu gezeichnet wird.
class ConfirmAction {
 public:
  enum Result { kNone, kArmed, kConfirmed };

  Result click() {
    if (armed_) {
      armed_ = false;
      left_ = 0.0f;
      return kConfirmed;
    }
    armed_ = true;
    left_ = -1.0f;  // Zeit setzt der nächste update()
    return kArmed;
  }
  // true = in diesem Frame von scharf auf ruhig gefallen (neu zeichnen).
  bool update(float dt, const ConfirmParams& p = {}) {
    if (!armed_) return false;
    if (left_ < 0.0f) left_ = p.timeout;
    if (!(dt > 0.0f) || !std::isfinite(dt)) dt = 0.0f;
    left_ -= dt;
    if (left_ > 0.0f) return false;
    armed_ = false;
    left_ = 0.0f;
    return true;
  }
  // Menü geschlossen, Seite gewechselt, Fokus verloren: nie scharf in einen neuen Kontext mitnehmen.
  void cancel() {
    armed_ = false;
    left_ = 0.0f;
  }
  bool armed() const { return armed_; }
  float remaining() const { return armed_ && left_ > 0.0f ? left_ : 0.0f; }

 private:
  float left_ = 0.0f;
  bool armed_ = false;
};

// Dauerdruck-Wiederholung (P2: ‹ › und Stick-Fokus). update() liefert, wie oft in diesem Frame ausgelöst
// wird: 1 auf der Druckflanke, danach nach `delay` alle `interval` Sekunden einmal. Nach reset() muss
// erst losgelassen werden (beim Öffnen gehaltene Tasten lösen nichts aus).
struct RepeatParams {
  float delay = 0.45f;
  float interval = 0.12f;
};

class HoldRepeat {
 public:
  int update(bool down, float dt, const RepeatParams& p = {}) {
    if (!armed_) {
      if (!down) armed_ = true;
      return 0;
    }
    if (!down) {
      down_ = false;
      return 0;
    }
    if (!(dt > 0.0f) || !std::isfinite(dt)) dt = 0.0f;
    if (!down_) {
      down_ = true;
      wait_ = p.delay;
      return 1;
    }
    wait_ -= dt;
    int fires = 0;
    while (wait_ <= 0.0f && fires < 4) {
      wait_ += p.interval > 0.0f ? p.interval : p.delay;
      ++fires;
    }
    return fires;
  }
  void reset() {
    armed_ = false;
    down_ = false;
  }
  bool down() const { return down_; }

 private:
  float wait_ = 0.0f;
  bool armed_ = true;
  bool down_ = false;
};

// Stick → Richtung für den Menüfokus: dominante Achse über der Schwelle, sonst (0, 0).
// Stick nach oben (y > 0) heißt im Menü eine Zeile höher (dy = −1).
inline void stickDirection(float x, float y, int* dx, int* dy, float threshold = 0.6f) {
  *dx = *dy = 0;
  if (std::fabs(x) < threshold && std::fabs(y) < threshold) return;
  if (std::fabs(x) >= std::fabs(y)) {
    *dx = x > 0.0f ? 1 : -1;
  } else {
    *dy = y > 0.0f ? -1 : 1;
  }
}

// Die drei Pflicht-„Aus“-Schalter. Gespeichert werden die Felder, abgefragt über die Funktionen.
struct PointerVisibility {
  bool ray = true;         // Laserstrahl
  bool marker = true;      // VR-Zielpunkt am Treffer
  bool gameCursor = true;  // spieleigener Mauszeiger

  // Im offenen Menü bleiben Strahl und Zielpunkt immer sichtbar (sonst ist das Menü unbedienbar).
  bool showRay(bool menuOpen) const { return menuOpen || ray; }
  bool showMarker(bool menuOpen) const { return menuOpen || marker; }
  bool showGameCursor(bool suppressed) const { return gameCursor && !suppressed; }
};

}  // namespace xrs
