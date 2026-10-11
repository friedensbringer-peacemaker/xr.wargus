#include "xr_theme.h"

#include <algorithm>
#include <cstdlib>

#include "xr_log.h"

namespace xrs {
namespace {

// Ersatzfarben, angelehnt an die Wargus-Menüs (auch für War1gus, dessen Menüs keine Knopfgrafiken haben).
constexpr Rgb kStone{52, 50, 48};
constexpr Rgb kStoneLight{104, 100, 94};
constexpr Rgb kStoneDark{18, 17, 16};
constexpr Rgb kHumanButton{30, 52, 116};
constexpr Rgb kOrcButton{96, 50, 26};
constexpr Rgb kGold{252, 212, 60};
constexpr Rgb kWhite{255, 255, 255};
constexpr Rgb kGray{150, 150, 150};
constexpr Rgb kDanger{150, 24, 18};
constexpr Rgb kDangerArmed{214, 32, 24};
constexpr Rgb kShadow{0, 0, 0};
// Schmalste Stauchung (Promille) für Beschriftungen, die in Schriftstufe 1 nicht passen.
constexpr int kMinSqueeze = 800;

// Schriftfarben aus wargus/scripts/fonts.lua = war1gus/scripts/fonts.lua (Palettenindizes 0–6 der Schriftgrafik).
constexpr Rgb kFontYellow[7] = {{252, 248, 240}, {244, 224, 32}, {208, 192, 28}, {168, 140, 16},
                                 {92, 48, 0},     {0, 0, 0},     {108, 108, 108}};
constexpr Rgb kFontWhite[7] = {{0, 0, 0},       {252, 248, 240}, {252, 248, 240}, {252, 248, 240},
                                {108, 108, 108}, {0, 0, 0},       {0, 0, 0}};
constexpr Rgb kFontGrey[7] = {{0, 0, 0},       {192, 192, 192}, {180, 180, 180}, {168, 168, 168},
                               {108, 108, 108}, {0, 0, 0},       {0, 0, 0}};

// Unicode → Codepage 437 (Zeichensatz der Wargus-/War1gus-Schriften); 0 = nicht vorhanden.
int toCp437(uint32_t cp) {
  if (cp >= 32 && cp < 127) return static_cast<int>(cp);
  switch (cp) {
    case 0xE4: return 0x84;  // ä
    case 0xF6: return 0x94;  // ö
    case 0xFC: return 0x81;  // ü
    case 0xC4: return 0x8E;  // Ä
    case 0xD6: return 0x99;  // Ö
    case 0xDC: return 0x9A;  // Ü
    case 0xDF: return 0xE1;  // ß
    case 0xE9: return 0x82;  // é
    case 0xB7: return '-';   // ·
    case 0xB0: return 0xF8;  // °
    case 0x2013: case 0x2014: return '-';
    case 0x2026: return '.';  // …
    case 0x2192: return '>';  // →
    case 0x203A: return '>';  // ›
    case 0x2039: return '<';  // ‹
    case 0x2212: return '-';  // −
    case 0x2190: return '<';  // ←
    default: return 0;
  }
}

uint32_t nextCodepoint(const std::string& s, size_t& i) {
  auto c = static_cast<unsigned char>(s[i++]);
  if (c < 0x80) return c;
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
  uint32_t cp = c & (0x3F >> extra);
  for (int k = 0; k < extra && i < s.size(); ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
  return cp;
}

std::string dataDir() {
  const char* d = std::getenv("XRS_DATA_DIR");
  return d && *d ? d : ".";
}

}  // namespace

Theme& Theme::get() {
  static Theme theme;
  return theme;
}

Theme::Theme() : game_(&currentGame()), cell_(game_->fontCell) {}

Image Theme::loadAsset(const char* pattern, std::vector<uint8_t>* index, int* key) const {
  const std::string rel = themeAsset(pattern, orc_);
  if (rel.empty()) return Image();  // Spielpaket hat keine solche Grafik → Ersatz
  return loadImage(dataDir() + "/" + rel, index, key);
}

void Theme::setOrc(bool orc) {
  if (loaded_ && orc == orc_) return;
  orc_ = orc;
  load();
}

void Theme::load() {
  loaded_ = true;
  panel_ = loadAsset(game_->panelImage);
  for (int i = 0; i < 3; ++i) large_[i] = loadAsset(game_->buttonImages[i]);

  if (!font_.valid()) {
    std::vector<uint8_t>& idx = font_.index;
    int key = 0;
    font_.glyphs = loadAsset(game_->fontImage, &idx, &key);
    font_.key = key;
    if (font_.glyphs.valid() && !idx.empty()) {
      font_.cellW = font_.cellH = cell_;
      font_.perRow = font_.glyphs.w / cell_;
      int count = font_.perRow * (font_.glyphs.h / cell_);
      font_.widths.assign(static_cast<size_t>(count), 0);
      // Glyphbreite wie CFont::MeasureWidths: rechteste Spalte mit Pixel ≠ Farbschlüssel/7.
      for (int g = 1; g < count; ++g) {
        int gx = (g % font_.perRow) * cell_, gy = (g / font_.perRow) * cell_;
        int width = 0;
        for (int y = 0; y < cell_; ++y) {
          for (int x = cell_ - 1; x >= 0; --x) {
            uint8_t v = idx[static_cast<size_t>(gy + y) * font_.glyphs.w + gx + x];
            if (v != key && v != 7) {
              width = std::max(width, x + 1);
              break;
            }
          }
        }
        font_.widths[static_cast<size_t>(g)] = width;
      }
      font_.widths[0] = cell_ / 2;  // Leerzeichen
    }
  }
  XLOGI("theme", "Menüstil %s (%s): Panel %s, Knöpfe %s, Schrift %s", orc_ ? "Ork" : "Mensch", game_->engineName,
        panel_.valid() ? "Spielgrafik" : "Ersatz", large_[0].valid() ? "Spielgrafik" : "Ersatz",
        font_.valid() ? "Spiel" : "System");
}

void Theme::drawPanel(Canvas& c, int x, int y, int w, int h) const {
  if (panel_.valid()) {
    c.drawImage(panel_, x, y, w, h);
    return;
  }
  c.bevel(x, y, w, h, kStone, kStoneLight, kStoneDark, false, 6);
}

void Theme::drawButton(Canvas& c, int x, int y, int w, int h, ButtonState state, const std::string& label) const {
  const Image& img = state == ButtonState::kPressed ? large_[1] : state == ButtonState::kGrayed ? large_[2] : large_[0];
  if (img.valid()) {
    c.drawImage(img, x, y, w, h);
  } else {
    Rgb face = orc_ ? kOrcButton : kHumanButton;
    if (state == ButtonState::kGrayed) face = {60, 60, 60};
    c.bevel(x, y, w, h, face, {static_cast<uint8_t>(std::min(255, face.r + 60)),
                               static_cast<uint8_t>(std::min(255, face.g + 60)),
                               static_cast<uint8_t>(std::min(255, face.b + 60))},
            kStoneDark, state == ButtonState::kPressed, 3);
  }
  int maxScale = std::max(1, (h - 8) / cell_);
  // Aktiv (gedrückt: gewählter Reiter, eingeschaltete Option) und unter dem Strahl: weiße Schrift,
  // wie die Hervorhebung der Spielmenüs. Das Wargus-Bild „gedrückt“ allein ist kaum zu unterscheiden.
  TextColor color = state == ButtonState::kHover || state == ButtonState::kPressed ? TextColor::kHighlight
                    : state == ButtonState::kGrayed                               ? TextColor::kGrayed
                                                                                  : TextColor::kNormal;
  // Einzelzeichen (Tastatur) groß, Wörter einheitlich mittelgroß.
  int cap = label.size() <= 2 ? 3 : 2;
  drawCentered(c, label, x + 6, y, w - 12, h, std::min(maxScale, cap), color);
}

void Theme::drawDangerButton(Canvas& c, int x, int y, int w, int h, bool armed, bool hover,
                             const std::string& label) const {
  int maxScale = std::max(1, std::min(2, (h - 8) / cell_));
  if (armed) {
    // Scharf: unübersehbar gefüllt, helle Kante, weiße Schrift.
    c.bevel(x, y, w, h, kDangerArmed, {255, 150, 130}, {90, 0, 0}, false, 4);
    drawCentered(c, label, x + 6, y, w - 12, h, maxScale, TextColor::kHighlight);
    return;
  }
  // Ruhig: Spiel-Schaltfläche rot eingefärbt (Ersatz: rote Fläche).
  if (large_[0].valid()) {
    c.drawImage(large_[0], x, y, w, h);
    c.tint(x, y, w, h, {255, 60, 50});
  } else {
    c.bevel(x, y, w, h, kDanger, {200, 90, 80}, kStoneDark, false, 3);
  }
  // Hellroter Rahmen: auch im Ork-Stil (rote Schaltflächen) eindeutig als gefährliche Aktion erkennbar.
  constexpr int kEdge = 3;
  c.fillRect(x, y, w, kEdge, kDangerArmed);
  c.fillRect(x, y + h - kEdge, w, kEdge, kDangerArmed);
  c.fillRect(x, y, kEdge, h, kDangerArmed);
  c.fillRect(x + w - kEdge, y, kEdge, h, kDangerArmed);
  drawCentered(c, label, x + 6, y, w - 12, h, maxScale, hover ? TextColor::kHighlight : TextColor::kNormal);
}

int Theme::lineHeight(int scale) const { return cell_ * scale; }

int Theme::textWidth(const std::string& text, int scale) const {
  if (!font_.valid()) {
    static Canvas probe(1, 1);  // nur zum Messen mit der Systemschrift
    return probe.textWidth(text, cell_ * scale * 9 / 10);
  }
  int w = 0;
  for (size_t i = 0; i < text.size();) {
    int cp = toCp437(nextCodepoint(text, i));
    int g = (cp ? cp : '?') - 32;
    if (g < 0 || g >= static_cast<int>(font_.widths.size())) continue;
    w += (font_.widths[static_cast<size_t>(g)] + 1) * scale;
  }
  return w;
}

int Theme::drawText(Canvas& c, const std::string& text, int x, int top, int scale, TextColor color) const {
  return drawText(c, text, x, top, scale, color, 1000);
}

int Theme::drawText(Canvas& c, const std::string& text, int x, int top, int scale, TextColor color,
                    int squeeze) const {
  if (!font_.valid()) {
    int size = cell_ * scale * 9 / 10;
    return c.drawShadowText(text, x, top + size, size,
                            color == TextColor::kHighlight ? kWhite : color == TextColor::kGrayed ? kGray : kGold,
                            kShadow);
  }
  // Spalte (ungestaucht, in Schriftpixeln) → Bildspalte; squeeze < 1000 lässt gleichmäßig Spalten aus.
  auto column = [&](int unscaled) { return x + unscaled * scale * squeeze / 1000; };
  int off = 0;
  for (size_t i = 0; i < text.size();) {
    int cp = toCp437(nextCodepoint(text, i));
    int g = (cp ? cp : '?') - 32;
    if (g < 0 || g >= static_cast<int>(font_.widths.size())) continue;
    int gw = font_.widths[static_cast<size_t>(g)];
    if (g > 0 && gw > 0) {
      // Wie Stratagus (font.cpp): Palettenindizes 0–6 der Schrift durch die Schriftfarbe ersetzen.
      const Rgb* colors = color == TextColor::kHighlight ? kFontWhite
                          : color == TextColor::kGrayed  ? kFontGrey
                                                         : kFontYellow;
      int sx = (g % font_.perRow) * font_.cellW, sy = (g / font_.perRow) * font_.cellH;
      for (int yy = 0; yy < font_.cellH; ++yy) {
        for (int xx = 0; xx < gw; ++xx) {
          size_t p = static_cast<size_t>(sy + yy) * font_.glyphs.w + sx + xx;
          int v = font_.index[p];
          if (v == font_.key) continue;
          Rgb col;
          if (v < 7) {
            col = colors[v];
          } else {
            const uint8_t* raw = &font_.glyphs.rgba[p * 4];
            col = {raw[0], raw[1], raw[2]};
          }
          int dx = column(off + xx), dw = column(off + xx + 1) - dx;
          if (dw > 0) c.fillRect(dx, top + yy * scale, dw, scale, col);
        }
      }
    }
    off += gw + 1;
  }
  return column(off) - x;
}

int Theme::fitScale(const std::string& text, int maxWidth, int maxScale) const {
  for (int s = maxScale; s > 1; --s) {
    if (textWidth(text, s) <= maxWidth) return s;
  }
  return 1;
}

void Theme::drawCentered(Canvas& c, const std::string& text, int x, int y, int w, int h, int maxScale,
                         TextColor color) const {
  int s = fitScale(text, w, maxScale);
  int tw = textWidth(text, s);
  int squeeze = 1000;
  if (s == 1 && tw > w && font_.valid()) {
    // Passt selbst in Stufe 1 nicht (breite Schrift, z. B. War1gus): bis kMinSqueeze schmaler zeichnen.
    squeeze = std::max(kMinSqueeze, w * 1000 / std::max(1, tw));
    tw = tw * squeeze / 1000;
  }
  drawText(c, text, x + (w - tw) / 2, y + (h - lineHeight(s)) / 2 + s, s, color, squeeze);
}

bool Theme::fits(const std::string& text, int maxWidth) const {
  int tw = textWidth(text, 1);
  return tw <= maxWidth || (font_.valid() && tw * kMinSqueeze / 1000 <= maxWidth);
}

}  // namespace xrs
