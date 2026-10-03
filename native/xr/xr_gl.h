// GL-Hilfen für die XR-Schicht. Alles läuft im GL-Kontext von SDL, der seinen Zustand
// cached: GlStateGuard sichert jeden Zustand, den die XR-Schicht anfasst, und stellt ihn
// exakt wieder her. Eigene Vertexdaten liegen in einem eigenen VAO (SDL nutzt VAO 0).
#pragma once

#include <GLES3/gl3.h>

namespace xrs {

class GlStateGuard {
 public:
  explicit GlStateGuard(bool srgbWriteControl);
  ~GlStateGuard();
  GlStateGuard(const GlStateGuard&) = delete;
  GlStateGuard& operator=(const GlStateGuard&) = delete;

 private:
  bool srgbControl_;
  GLint program_, arrayBuffer_, vao_, activeTexture_, texture0_, readFb_, drawFb_, unpackBuffer_,
      unpackAlignment_, unpackRowLength_;
  GLint viewport_[4], scissorBox_[4];
  GLboolean scissor_, blend_, depth_, cull_, srgb_;
  GLboolean colorMask_[4];
  GLint blendSrcRgb_, blendDstRgb_, blendSrcAlpha_, blendDstAlpha_, blendEqRgb_, blendEqAlpha_;
  GLfloat clearColor_[4];
};

// Shader + VAO für Strahl und Zielpunkt (Billboards, vormultipliziertes Alpha).
class BeamRenderer {
 public:
  enum Mode { kBeam = 0, kRing = 1, kDot = 2 };
  bool init();
  // vertices: 4 Ecken (x,y,z,u,v) als Triangle-Strip, mvp spaltenweise.
  void draw(const float* mvp, const float* vertices, const float color[4], Mode mode);

 private:
  GLuint program_ = 0, vao_ = 0, vbo_ = 0;
  GLint mvpLoc_ = -1, colorLoc_ = -1, modeLoc_ = -1;
};

// Zeichnet eine Textur bildschirmfüllend mit Bildreglern (Helligkeit, Kontrast, Sättigung,
// Schärfe; Werte 0–100, 50 bzw. Schärfe 0 = unverändert). Rechnet auf den sRGB-kodierten
// Werten, wie sie aus dem Spiel kommen.
class FilterRenderer {
 public:
  bool init();
  // crt: 0 normal, 1 Scanlines, 2 CRT; outWidth/outHeight = Größe des Ziels (Scanline-Raster)
  void draw(GLuint texture, int width, int height, int brightness, int contrast, int saturation, int sharpness,
            int crt, int outWidth, int outHeight);

 private:
  GLuint program_ = 0, vao_ = 0;
  GLint texLoc_ = -1, texelLoc_ = -1, brightLoc_ = -1, contrastLoc_ = -1, satLoc_ = -1, sharpLoc_ = -1, crtLoc_ = -1,
        sizeLoc_ = -1;
};

}  // namespace xrs
