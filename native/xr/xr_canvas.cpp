#include "xr_canvas.h"

#include <SDL.h>
#include <SDL_image.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "xr_log.h"

namespace xrs {
namespace {

const char* kFonts[] = {"/system/fonts/Roboto-Regular.ttf", "/system/fonts/RobotoStatic-Regular.ttf",
                        "/system/fonts/DroidSans.ttf"};

// Eine Schrift für alle Zeichenflächen, beim ersten Gebrauch geladen (nur XR-Thread).
FT_Face font() {
  static bool tried = false;
  static FT_Face face = nullptr;
  if (tried) return face;
  tried = true;
  FT_Library lib = nullptr;
  if (FT_Init_FreeType(&lib) != 0) {
    XLOGE("canvas", "FreeType nicht initialisierbar");
    return nullptr;
  }
  // XRS_FONT: andere Schrift (Desktop-Vorschau tools/menu-preview).
  const char* custom = std::getenv("XRS_FONT");
  if (custom && *custom && FT_New_Face(lib, custom, 0, &face) == 0) {
    XLOGI("canvas", "Schrift: %s", custom);
    return face;
  }
  for (const char* path : kFonts) {
    if (FT_New_Face(lib, path, 0, &face) == 0) {
      XLOGI("canvas", "Schrift: %s", path);
      return face;
    }
    face = nullptr;
  }
  XLOGE("canvas", "keine Systemschrift gefunden – Anzeigen ohne Text");
  return nullptr;
}

uint32_t nextCodepoint(const std::string& s, size_t& i) {
  auto c = static_cast<unsigned char>(s[i++]);
  if (c < 0x80) return c;
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
  uint32_t cp = c & (0x3F >> extra);
  for (int k = 0; k < extra && i < s.size(); ++k) {
    cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
  }
  return cp;
}

}  // namespace

Image loadImage(const std::string& path, std::vector<uint8_t>* indices, int* colorKey) {
  Image img;
  SDL_Surface* raw = IMG_Load(path.c_str());
  if (!raw) {
    XLOGW("canvas", "Grafik nicht ladbar: %s (%s)", path.c_str(), IMG_GetError());
    return img;
  }
  // Paletten-Bilder: Farbschlüssel wie Stratagus (gesetzt vom PNG, sonst Index 0).
  int key = -1;
  if (raw->format->BytesPerPixel == 1) {
    Uint32 ck = 0;
    key = SDL_GetColorKey(raw, &ck) == 0 ? static_cast<int>(ck) : 0;
    if (indices) {
      indices->resize(static_cast<size_t>(raw->w) * raw->h);
      SDL_LockSurface(raw);
      for (int y = 0; y < raw->h; ++y) {
        std::memcpy(&(*indices)[static_cast<size_t>(y) * raw->w],
                    static_cast<const uint8_t*>(raw->pixels) + static_cast<size_t>(y) * raw->pitch,
                    static_cast<size_t>(raw->w));
      }
      SDL_UnlockSurface(raw);
    }
  }
  if (colorKey) *colorKey = key;
  SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
  if (!rgba) {
    SDL_FreeSurface(raw);
    return img;
  }
  img.w = rgba->w;
  img.h = rgba->h;
  img.rgba.resize(static_cast<size_t>(img.w) * img.h * 4);
  SDL_LockSurface(rgba);
  for (int y = 0; y < img.h; ++y) {
    std::memcpy(&img.rgba[static_cast<size_t>(y) * img.w * 4],
                static_cast<const uint8_t*>(rgba->pixels) + static_cast<size_t>(y) * rgba->pitch,
                static_cast<size_t>(img.w) * 4);
  }
  SDL_UnlockSurface(rgba);
  if (key >= 0) {
    SDL_LockSurface(raw);
    for (int y = 0; y < img.h; ++y) {
      const uint8_t* src = static_cast<const uint8_t*>(raw->pixels) + static_cast<size_t>(y) * raw->pitch;
      for (int x = 0; x < img.w; ++x) {
        if (src[x] == key) img.rgba[(static_cast<size_t>(y) * img.w + x) * 4 + 3] = 0;
      }
    }
    SDL_UnlockSurface(raw);
  }
  SDL_FreeSurface(rgba);
  SDL_FreeSurface(raw);
  return img;
}

void Canvas::drawImage(const Image& img, int sx, int sy, int sw, int sh, int x, int y, int w, int h, int tint) {
  if (!img.valid() || w <= 0 || h <= 0 || sw <= 0 || sh <= 0) return;
  for (int yy = std::max(0, y); yy < std::min(height_, y + h); ++yy) {
    int srcY = sy + (yy - y) * sh / h;
    if (srcY < 0 || srcY >= img.h) continue;
    uint8_t* dst = &pixels_[static_cast<size_t>(yy) * width_ * 4];
    for (int xx = std::max(0, x); xx < std::min(width_, x + w); ++xx) {
      int srcX = sx + (xx - x) * sw / w;
      if (srcX < 0 || srcX >= img.w) continue;
      const uint8_t* s = &img.rgba[(static_cast<size_t>(srcY) * img.w + srcX) * 4];
      if (s[3] < 128) continue;
      uint8_t* d = dst + static_cast<size_t>(xx) * 4;
      if (tint == 1) {
        // Hervorhebung: helle Schriftpixel weiß, dunkle Kontur bleibt.
        int lum = (s[0] * 3 + s[1] * 6 + s[2]) / 10;
        uint8_t v = lum > 90 ? 255 : static_cast<uint8_t>(lum);
        d[0] = d[1] = d[2] = v;
      } else {
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
      }
      d[3] = 255;
    }
  }
}

void Canvas::tint(int x, int y, int w, int h, Rgb color) {
  for (int yy = std::max(0, y); yy < std::min(height_, y + h); ++yy) {
    for (int xx = std::max(0, x); xx < std::min(width_, x + w); ++xx) {
      uint8_t* p = &pixels_[(static_cast<size_t>(yy) * width_ + xx) * 4];
      int lum = (p[0] * 3 + p[1] * 6 + p[2]) / 10;
      p[0] = static_cast<uint8_t>(std::min(255, lum * color.r / 128));
      p[1] = static_cast<uint8_t>(std::min(255, lum * color.g / 128));
      p[2] = static_cast<uint8_t>(std::min(255, lum * color.b / 128));
    }
  }
}

Canvas::Canvas(int width, int height)
    : width_(width), height_(height), pixels_(static_cast<size_t>(width) * height * 4, 255) {}

bool Canvas::hasFont() { return font() != nullptr; }

void Canvas::fillRect(int x, int y, int w, int h, Rgb c) {
  int x0 = std::max(0, x), y0 = std::max(0, y);
  int x1 = std::min(width_, x + w), y1 = std::min(height_, y + h);
  for (int yy = y0; yy < y1; ++yy) {
    uint8_t* p = &pixels_[(static_cast<size_t>(yy) * width_ + x0) * 4];
    for (int xx = x0; xx < x1; ++xx, p += 4) {
      p[0] = c.r;
      p[1] = c.g;
      p[2] = c.b;
      p[3] = 255;
    }
  }
}

void Canvas::bevel(int x, int y, int w, int h, Rgb face, Rgb light, Rgb dark, bool sunken, int edge) {
  Rgb top = sunken ? dark : light;
  Rgb bottom = sunken ? light : dark;
  fillRect(x, y, w, h, face);
  fillRect(x, y, w, edge, top);
  fillRect(x, y, edge, h, top);
  fillRect(x, y + h - edge, w, edge, bottom);
  fillRect(x + w - edge, y, edge, h, bottom);
}

int Canvas::drawShadowText(const std::string& text, int x, int baseline, int sizePx, Rgb c, Rgb shadow) {
  int offset = std::max(1, sizePx / 16);
  drawText(text, x + offset, baseline + offset, sizePx, shadow);
  return drawText(text, x, baseline, sizePx, c);
}

int Canvas::textWidth(const std::string& text, int sizePx) {
  FT_Face face = font();
  if (!face) return 0;
  FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(sizePx));
  int w = 0;
  for (size_t i = 0; i < text.size();) {
    if (FT_Load_Char(face, nextCodepoint(text, i), FT_LOAD_DEFAULT) == 0) {
      w += static_cast<int>(face->glyph->advance.x >> 6);
    }
  }
  return w;
}

int Canvas::fitSize(const std::string& text, int maxWidth, int maxSize, int minSize) {
  int size = maxSize;
  while (size > minSize && textWidth(text, size) > maxWidth) size -= 2;
  return size;
}

std::vector<std::string> Canvas::wrap(const std::string& text, int maxWidth, int sizePx) {
  std::vector<std::string> lines;
  std::string line;
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t space = text.find(' ', pos);
    std::string word = text.substr(pos, space == std::string::npos ? std::string::npos : space - pos);
    std::string candidate = line.empty() ? word : line + " " + word;
    if (!line.empty() && textWidth(candidate, sizePx) > maxWidth) {
      lines.push_back(line);
      line = word;
    } else {
      line = candidate;
    }
    if (space == std::string::npos) break;
    pos = space + 1;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

int Canvas::drawText(const std::string& text, int x, int baseline, int sizePx, Rgb c) {
  FT_Face face = font();
  if (!face) return 0;
  FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(sizePx));
  const uint8_t rgb[3] = {c.r, c.g, c.b};
  int penX = x;
  for (size_t i = 0; i < text.size();) {
    if (FT_Load_Char(face, nextCodepoint(text, i), FT_LOAD_RENDER) != 0) continue;
    FT_GlyphSlot g = face->glyph;
    const FT_Bitmap& bm = g->bitmap;
    int left = penX + g->bitmap_left;
    int top = baseline - g->bitmap_top;
    for (unsigned row = 0; row < bm.rows; ++row) {
      int y = top + static_cast<int>(row);
      if (y < 0 || y >= height_) continue;
      for (unsigned col = 0; col < bm.width; ++col) {
        int xx = left + static_cast<int>(col);
        if (xx < 0 || xx >= width_) continue;
        unsigned a = bm.buffer[row * static_cast<unsigned>(bm.pitch) + col];
        if (!a) continue;
        uint8_t* p = &pixels_[(static_cast<size_t>(y) * width_ + xx) * 4];
        for (int k = 0; k < 3; ++k) {
          p[k] = static_cast<uint8_t>((rgb[k] * a + p[k] * (255 - a)) / 255);
        }
      }
    }
    penX += static_cast<int>(g->advance.x >> 6);
  }
  return penX - x;
}

}  // namespace xrs
