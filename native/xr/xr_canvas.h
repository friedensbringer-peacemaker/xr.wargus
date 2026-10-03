// CPU-Zeichenfläche (RGBA, Zeile 0 = oben) mit Text über FreeType und die Systemschrift
// der Quest. Grundlage für VR-Menü und Ladeanzeige; läuft nur im XR-Thread.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xrs {

struct Rgb {
  uint8_t r, g, b;
};

// RGBA-Bild (Zeile 0 = oben), z. B. eine Spielgrafik.
struct Image {
  int w = 0, h = 0;
  std::vector<uint8_t> rgba;
  bool valid() const { return w > 0 && h > 0 && rgba.size() == static_cast<size_t>(w) * h * 4; }
};

// PNG laden (SDL_image). Paletten-Bilder: Farbschlüssel bzw. Index 0 wird durchsichtig.
// palette (optional) erhält die Farbindizes, z. B. für die Glyphbreiten der Spielschrift.
Image loadImage(const std::string& path, std::vector<uint8_t>* indices = nullptr, int* colorKey = nullptr);

class Canvas {
 public:
  Canvas(int width, int height);
  int width() const { return width_; }
  int height() const { return height_; }
  const uint8_t* data() const { return pixels_.data(); }

  void fill(Rgb c) { fillRect(0, 0, width_, height_, c); }
  // Bildausschnitt (sx,sy,sw,sh) nach (x,y,w,h) skalieren (nächster Nachbar); Alpha < 128 bleibt frei.
  // tint: 0 = Originalfarben, 1 = helle Pixel weiß (Hervorhebung der Spielschrift).
  void drawImage(const Image& img, int sx, int sy, int sw, int sh, int x, int y, int w, int h, int tint = 0);
  void drawImage(const Image& img, int x, int y, int w, int h) { drawImage(img, 0, 0, img.w, img.h, x, y, w, h); }
  // Fläche einfärben: Helligkeit je Pixel × Farbe/128 (z. B. rote Gefahr-Schaltfläche aus dem Spielbild).
  void tint(int x, int y, int w, int h, Rgb color);
  void fillRect(int x, int y, int w, int h, Rgb c);
  // Fläche mit Kanten (hell oben/links, dunkel unten/rechts;
  // sunken = eingedrückt: Kanten vertauscht).
  void bevel(int x, int y, int w, int h, Rgb face, Rgb light, Rgb dark, bool sunken = false, int edge = 3);
  // Text mit dunklem Schatten (Lesbarkeit auf hellen Flächen, wie die Spielschrift).
  int drawShadowText(const std::string& text, int x, int baseline, int sizePx, Rgb c, Rgb shadow);
  // Text: x = linker Rand, baseline = Grundlinie. Rückgabe: Breite in Pixeln.
  int drawText(const std::string& text, int x, int baseline, int sizePx, Rgb c);
  int textWidth(const std::string& text, int sizePx);
  // Größte Schriftgröße <= maxSize, bei der text in maxWidth passt (mind. minSize).
  int fitSize(const std::string& text, int maxWidth, int maxSize, int minSize);
  // Zeilenumbruch an Leerzeichen für die gegebene Breite.
  std::vector<std::string> wrap(const std::string& text, int maxWidth, int sizePx);

  static bool hasFont();

 private:
  int width_, height_;
  std::vector<uint8_t> pixels_;
};

}  // namespace xrs
