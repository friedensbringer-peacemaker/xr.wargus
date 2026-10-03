// Eingabe-Bausteine nach den Regeln aus dem XR-Port-Lernstand (XR-Settlers2.5):
// Hysterese für analoge Tasten, Kurz-/Langdruck ohne Doppelauslösung, Re-Arm nach
// dem Lösen gehaltener Eingaben, zeitabhängige Glättung.
#pragma once

#include <cmath>

namespace xrs {

// Analoger Trigger/Griff als Taste mit Hysterese. Nach reset() muss erst losgelassen
// werden (re-arm), damit ein noch gehaltener Trigger nie als neuer Druck zählt.
class AnalogButton {
 public:
  // Liefert +1 bei Druck, -1 beim Loslassen, sonst 0.
  int update(float value) {
    if (!armed_) {
      if (value < kRelease) armed_ = true;
      return 0;
    }
    if (!down_ && value > kPress) {
      down_ = true;
      return 1;
    }
    if (down_ && value < kRelease) {
      down_ = false;
      return -1;
    }
    return 0;
  }
  bool down() const { return down_; }
  void reset() {
    down_ = false;
    armed_ = false;
  }

 private:
  static constexpr float kPress = 0.55f;
  static constexpr float kRelease = 0.35f;
  bool down_ = false;
  bool armed_ = true;
};

// Digitale Taste mit Kurz-/Langdruck. Ist Lang belegt, feuert Kurz erst beim
// Loslassen (vor Ablauf der Langdruckzeit), nie beides.
class PressButton {
 public:
  enum Event { kNone, kShort, kLong };

  Event update(bool pressed, float dt) {
    if (!armed_) {
      if (!pressed) armed_ = true;
      return kNone;
    }
    if (pressed) {
      if (!down_) {
        down_ = true;
        held_ = 0.0f;
        longFired_ = false;
      } else {
        held_ += dt;
        if (!longFired_ && held_ >= kLongPress) {
          longFired_ = true;
          return kLong;
        }
      }
      return kNone;
    }
    Event e = (down_ && !longFired_) ? kShort : kNone;
    down_ = false;
    return e;
  }
  void reset() {
    down_ = false;
    armed_ = false;
  }

 private:
  static constexpr float kLongPress = 0.6f;
  bool down_ = false;
  bool armed_ = true;
  bool longFired_ = false;
  float held_ = 0.0f;
};

// Stick-Achse mit Deadzone 0,25, danach auf 0..1 umskaliert.
inline float applyDeadzone(float v) {
  constexpr float kDead = 0.25f;
  float a = std::fabs(v);
  if (a < kDead) return 0.0f;
  float s = (a - kDead) / (1.0f - kDead);
  return v < 0.0f ? -s : s;
}

// Exponentielle Glättung, zeitabhängig (nicht pro Frame).
inline float smoothingAlpha(float dt, float tau) { return 1.0f - std::exp(-dt / tau); }

}  // namespace xrs
