// VR-Tastatur (QWERTZ, Umlaute): erscheint, sobald die Engine Texteingabe anfordert
// (SDL_StartTextInput, z. B. Klick in „Neuer Spielstand“), und verschwindet mit
// SDL_StopTextInput. Die Android-Bildschirmtastatur wird in der immersiven App nicht angezeigt.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "xr_canvas.h"

namespace xrs {

enum class KeyKind { kText, kBackspace, kShift, kSpace, kSuggest, kCancel, kConfirm };

struct KeyHit {
  KeyKind kind;
  std::string text;  // bei kText: einzufügender Text (Umschalt schon berücksichtigt)
};

class KeyboardPanel {
 public:
  static constexpr int kWidth = 1280;
  static constexpr int kHeight = 520;

  KeyboardPanel() : canvas_(kWidth, kHeight) { layout(); }
  void setHover(int index);
  int hit(float u, float v) const;
  // Liefert die Wirkung einer Taste; Umschalt schaltet intern um (einmalig bis zum nächsten Zeichen).
  KeyHit press(int index);
  void setShift(bool shift);
  const uint8_t* pixels();
  uint32_t version() const { return version_; }

  // Für Tests (tests/test_ui.cpp).
  int count() const { return static_cast<int>(keys_.size()); }
  void rect(int index, int* x, int* y, int* w, int* h) const {
    const Key& k = keys_[static_cast<size_t>(index)];
    *x = k.x; *y = k.y; *w = k.w; *h = k.h;
  }
  std::string label(int index) const {
    const Key& k = keys_[static_cast<size_t>(index)];
    return shift_ ? k.upper : k.lower;
  }

 private:
  struct Key {
    KeyKind kind;
    std::string lower, upper;
    int x, y, w, h;
  };
  void layout();
  void render();

  Canvas canvas_;
  std::vector<Key> keys_;
  bool shift_ = false;
  int hover_ = -1;
  bool dirty_ = true;
  uint32_t version_ = 0;
};

}  // namespace xrs
