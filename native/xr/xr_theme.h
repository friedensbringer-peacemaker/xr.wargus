// Menüstil wie die Spielmenüs von Wargus bzw. War1gus: Panel, Schaltflächen und goldene Menüschrift
// werden zur Laufzeit aus den eigenen Spieldaten des Spielers geladen (Pfade je Spielpaket in
// xr_game.h, nie in der APK). Fehlt eine Grafik (War1gus hat z. B. keine Knopfgrafiken), zeichnet
// der Stil ein ähnliches Aussehen selbst (Stein-Grau, Blau/Braun, Gold), ohne Spielschrift mit der
// Systemschrift. Nur im XR-Thread benutzen.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "xr_canvas.h"
#include "xr_game.h"

namespace xrs {

enum class ButtonState { kNormal, kHover, kPressed, kGrayed };
// Schriftfarben der Wargus-Menüs (fonts.lua): gelb, weiß (Hervorhebung), grau (gesperrt).
enum class TextColor { kNormal, kHighlight, kGrayed };

class Theme {
 public:
  static Theme& get();

  // Menschen- (blau) oder Ork-Stil (braun); lädt die passenden Grafiken nach.
  void setOrc(bool orc);
  bool orc() const { return orc_; }
  // Spielgrafiken gefunden (sonst gezeichneter Ersatz).
  bool assets() const { return panel_.valid(); }
  // Schaltflächen aus Spielgrafiken (sonst gezeichneter Ersatz).
  bool buttonAssets() const { return large_[0].valid(); }
  // Menüschrift aus den Spieldaten geladen (sonst Systemschrift).
  bool gameFont() const { return font_.valid(); }

  void drawPanel(Canvas& c, int x, int y, int w, int h) const;
  void drawButton(Canvas& c, int x, int y, int w, int h, ButtonState state, const std::string& label) const;
  // Gefährliche Aktion (Beenden): rot; scharf = gefüllt hellrot mit weißer Schrift (P3, Skill xr-vr-menu).
  void drawDangerButton(Canvas& c, int x, int y, int w, int h, bool armed, bool hover,
                        const std::string& label) const;
  // Text in Menüschrift; top = Oberkante. Rückgabe: Breite.
  int drawText(Canvas& c, const std::string& text, int x, int top, int scale, TextColor color) const;
  int textWidth(const std::string& text, int scale) const;
  int lineHeight(int scale) const;
  // Größte Schriftstufe (≤ maxScale), in der text in maxWidth passt; 1 notfalls.
  int fitScale(const std::string& text, int maxWidth, int maxScale) const;
  // Passt text in maxWidth – in Stufe 1 oder (Spielschrift) bis 80 % gestaucht wie in drawCentered?
  bool fits(const std::string& text, int maxWidth) const;
  // Zentriert in ein Rechteck.
  void drawCentered(Canvas& c, const std::string& text, int x, int y, int w, int h, int maxScale,
                    TextColor color) const;

 private:
  Theme();
  void load();
  int drawText(Canvas& c, const std::string& text, int x, int top, int scale, TextColor color, int squeeze) const;
  Image loadAsset(const char* pattern, std::vector<uint8_t>* index = nullptr, int* key = nullptr) const;

  struct Font {
    Image glyphs;
    std::vector<uint8_t> index;  // Palettenindex je Pixel
    int key = 0;                 // Farbschlüssel (durchsichtig)
    int cellW = 0, cellH = 0, perRow = 0;
    std::vector<int> widths;  // je Glyph (Index = Codepage-437-Zeichen − 32)
    bool valid() const { return glyphs.valid() && perRow > 0; }
  };

  const GameProfile* game_;
  int cell_;  // Glyphenzelle der Menüschrift (Pixel), bestimmt auch Zeilenhöhe und Schriftstufen
  bool orc_ = false;
  bool loaded_ = false;
  Image panel_;
  Image large_[3];  // normal, pressed, grayed
  Font font_;
};

}  // namespace xrs
