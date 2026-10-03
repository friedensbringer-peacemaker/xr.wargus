// Bildfilter der XR-Schicht (native/xr/xr_gl.cpp, FilterRenderer) auf der Quest-GPU: Shader kompilieren
// und linken, Testbild in den Modi normal/Scanlines/CRT zeichnen, Pixel prüfen und als PPM ablegen.
// Läuft als ARM64-Programm mit EGL-Pbuffer (ohne Fenster, ohne App).
//
//   test_filter_gpu <ausgabeordner>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "xr_gl.h"
#include "xr_log.h"

namespace xrs {
void logf(LogLevel, const char* area, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::fprintf(stderr, "%s: ", area);
  std::vfprintf(stderr, fmt, args);
  std::fputc('\n', stderr);
  va_end(args);
}
}  // namespace xrs

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
  std::printf("%s: %s\n", ok ? "ok" : "FEHLER", what.c_str());
  if (!ok) ++g_failures;
}

constexpr int kW = 1280, kH = 800;

void savePpm(const std::vector<unsigned char>& rgba, const std::string& file) {
  FILE* f = std::fopen(file.c_str(), "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", kW, kH);
  for (int y = kH - 1; y >= 0; --y) {  // GL-Zeilen beginnen unten
    for (int x = 0; x < kW; ++x) std::fwrite(&rgba[(static_cast<size_t>(y) * kW + x) * 4], 1, 3, f);
  }
  std::fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : ".";
  EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(dpy, nullptr, nullptr);
  const EGLint cfgAttr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
  EGLConfig cfg;
  EGLint n = 0;
  eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n);
  const EGLint pbAttr[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
  EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pbAttr);
  const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
  check(n > 0 && ctx != EGL_NO_CONTEXT && eglMakeCurrent(dpy, surf, surf, ctx), "EGL-Kontext (GLES 3)");
  if (g_failures) return 1;

  xrs::FilterRenderer filter;
  check(filter.init(), "Filter-Shader kompiliert und gelinkt");
  if (g_failures) return 1;

  // Testbild: Farbverlauf mit grauer Fläche in der Mitte (Werte prüfbar)
  std::vector<unsigned char> src(static_cast<size_t>(kW) * kH * 4);
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) {
      unsigned char* p = &src[(static_cast<size_t>(y) * kW + x) * 4];
      bool mid = x > kW / 3 && x < 2 * kW / 3 && y > kH / 3 && y < 2 * kH / 3;
      p[0] = mid ? 128 : static_cast<unsigned char>(x * 255 / kW);
      p[1] = mid ? 128 : static_cast<unsigned char>(y * 255 / kH);
      p[2] = mid ? 128 : 160;
      p[3] = 255;
    }
  }
  GLuint tex, dst, fbo;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kW, kH, 0, GL_RGBA, GL_UNSIGNED_BYTE, src.data());
  glGenTextures(1, &dst);
  glBindTexture(GL_TEXTURE_2D, dst);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kW, kH);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst, 0);
  check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Ziel-FBO vollständig");

  const char* names[] = {"normal", "scanlines", "crt"};
  std::vector<unsigned char> px(src.size());
  auto at = [&](int x, int y, int c) { return static_cast<int>(px[(static_cast<size_t>(y) * kW + x) * 4 + c]); };
  for (int mode = 0; mode < 3; ++mode) {
    glViewport(0, 0, kW, kH);
    filter.draw(tex, kW, kH, 50, 50, 50, 0, mode, kW, kH);
    glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    check(glGetError() == GL_NO_ERROR, std::string(names[mode]) + ": ohne GL-Fehler");
    // Mittelfläche (grau 128): Zeilenpaar y=400/401
    int a = at(kW / 2, 400, 1), b = at(kW / 2, 401, 1);
    char what[160];
    std::snprintf(what, sizeof(what), "%s: Mitte Zeile 400/401 = %d/%d", names[mode], a, b);
    if (mode == 0) {
      check(std::abs(a - 128) <= 2 && std::abs(b - 128) <= 2, what);
    } else {
      // Scanlines: eine Zeile hell (×1.18), eine dunkel (×0.62×1.18), Verhältnis ~0.62
      int hi = a > b ? a : b, lo = a > b ? b : a;
      // (CRT hebt die Mitte über Vignette/Maske zusätzlich an: nur Verhältnis und Abstand prüfen)
      check(hi > lo + 30 && std::fabs(static_cast<double>(lo) / hi - 0.62) < 0.05, what);
    }
    if (mode == 2) {
      // Vignette: Ecke dunkler als die Mitte bei gleicher Quelle? Quelle unterscheidet sich – Maske prüfen:
      int r = at(kW / 2, 400, 0), g = at(kW / 2 + 1, 400, 0), bl = at(kW / 2 + 2, 400, 0);
      std::snprintf(what, sizeof(what), "crt: Streifenmaske Rotkanal %d/%d/%d", r, g, bl);
      check(!(r == g && g == bl), what);
    }
    savePpm(px, out + "/filter-" + names[mode] + ".ppm");
  }
  std::printf("test_filter_gpu: %d Fehler\n", g_failures);
  return g_failures ? 1 : 0;
}
