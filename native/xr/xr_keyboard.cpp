#include "xr_keyboard.h"

#include "xr_theme.h"

namespace xrs {
namespace {

constexpr int kMargin = 20;
constexpr int kGap = 8;
constexpr int kTitleH = 60;


// Zeichenreihen (klein / groß), jeweils durch Leerzeichen getrennt.
const char* kRowsLower[] = {"1 2 3 4 5 6 7 8 9 0 ß", "q w e r t z u i o p ü", "a s d f g h j k l ö ä",
                            "y x c v b n m , . - _"};
const char* kRowsUpper[] = {"! \" § $ % & / ( ) = ?", "Q W E R T Z U I O P Ü", "A S D F G H J K L Ö Ä",
                            "Y X C V B N M ; : + #"};

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t pos = 0;
  while (pos < s.size()) {
    size_t sp = s.find(' ', pos);
    out.push_back(s.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos));
    if (sp == std::string::npos) break;
    pos = sp + 1;
  }
  return out;
}

}  // namespace

void KeyboardPanel::layout() {
  keys_.clear();
  const int contentW = kWidth - 2 * kMargin;
  const int rows = 5;
  const int keyH = (kHeight - kTitleH - kMargin - (rows - 1) * kGap) / rows;
  const int unit = (contentW - 11 * kGap) / 12;  // 12 Einheiten pro Reihe
  for (int r = 0; r < 4; ++r) {
    std::vector<std::string> lower = split(kRowsLower[r]);
    std::vector<std::string> upper = split(kRowsUpper[r]);
    int y = kTitleH + r * (keyH + kGap);
    int x = kMargin + (r == 3 ? unit + kGap : 0);
    if (r == 3) keys_.push_back({KeyKind::kShift, "Umschalt", "Umschalt", kMargin, y, unit, keyH});
    for (size_t i = 0; i < lower.size(); ++i) {
      keys_.push_back({KeyKind::kText, lower[i], upper[i], x, y, unit, keyH});
      x += unit + kGap;
    }
    if (r == 0) keys_.push_back({KeyKind::kBackspace, "← Löschen", "← Löschen", x, y, kWidth - kMargin - x, keyH});
  }
  int y = kTitleH + 4 * (keyH + kGap);
  int w3 = unit * 3 + 2 * kGap;
  int spaceW = contentW - 3 * w3 - 3 * kGap;
  int x = kMargin;
  keys_.push_back({KeyKind::kSuggest, "Name vorschlagen", "Name vorschlagen", x, y, w3, keyH});
  x += w3 + kGap;
  keys_.push_back({KeyKind::kSpace, "Leertaste", "Leertaste", x, y, spaceW, keyH});
  x += spaceW + kGap;
  keys_.push_back({KeyKind::kCancel, "Abbrechen", "Abbrechen", x, y, w3, keyH});
  x += w3 + kGap;
  keys_.push_back({KeyKind::kConfirm, "Übernehmen", "Übernehmen", x, y, kWidth - kMargin - x, keyH});
  dirty_ = true;
}

void KeyboardPanel::setHover(int index) {
  if (index != hover_) {
    hover_ = index;
    dirty_ = true;
  }
}

void KeyboardPanel::setShift(bool shift) {
  if (shift != shift_) {
    shift_ = shift;
    dirty_ = true;
  }
}

int KeyboardPanel::hit(float u, float v) const {
  int x = static_cast<int>(u * kWidth);
  int y = static_cast<int>(v * kHeight);
  for (size_t i = 0; i < keys_.size(); ++i) {
    const Key& k = keys_[i];
    if (x >= k.x && x < k.x + k.w && y >= k.y && y < k.y + k.h) return static_cast<int>(i);
  }
  return -1;
}

KeyHit KeyboardPanel::press(int index) {
  const Key& k = keys_[static_cast<size_t>(index)];
  switch (k.kind) {
    case KeyKind::kShift:
      setShift(!shift_);
      return {KeyKind::kShift, ""};
    case KeyKind::kText: {
      std::string text = shift_ ? k.upper : k.lower;
      setShift(false);  // Umschalt gilt für ein Zeichen
      return {KeyKind::kText, text};
    }
    case KeyKind::kSpace:
      return {KeyKind::kText, " "};
    default:
      return {k.kind, ""};
  }
}

const uint8_t* KeyboardPanel::pixels() {
  if (dirty_) {
    render();
    dirty_ = false;
    ++version_;
  }
  return canvas_.data();
}

void KeyboardPanel::render() {
  const Theme& theme = Theme::get();
  theme.drawPanel(canvas_, 0, 0, kWidth, kHeight);
  theme.drawCentered(canvas_, "Texteingabe - Tasten mit dem Strahl antippen", kMargin, 12, kWidth - 2 * kMargin,
                     kTitleH - 20, 2, TextColor::kNormal);
  for (size_t i = 0; i < keys_.size(); ++i) {
    const Key& k = keys_[i];
    bool hot = static_cast<int>(i) == hover_;
    bool shiftOn = k.kind == KeyKind::kShift && shift_;
    ButtonState st = shiftOn ? ButtonState::kPressed : hot ? ButtonState::kHover : ButtonState::kNormal;
    theme.drawButton(canvas_, k.x, k.y, k.w, k.h, st, shift_ ? k.upper : k.lower);
  }
}

}  // namespace xrs
