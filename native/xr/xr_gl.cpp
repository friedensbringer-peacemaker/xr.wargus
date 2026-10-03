#include "xr_gl.h"

// gl2ext.h braucht die Typen aus gl3.h (Reihenfolge nicht umsortieren).
#include <GLES2/gl2ext.h>

#include "xr_log.h"

namespace xrs {

GlStateGuard::GlStateGuard(bool srgbWriteControl) : srgbControl_(srgbWriteControl) {
  glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer_);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0_);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFb_);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFb_);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer_);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment_);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpackRowLength_);
  glGetIntegerv(GL_VIEWPORT, viewport_);
  glGetIntegerv(GL_SCISSOR_BOX, scissorBox_);
  scissor_ = glIsEnabled(GL_SCISSOR_TEST);
  blend_ = glIsEnabled(GL_BLEND);
  depth_ = glIsEnabled(GL_DEPTH_TEST);
  cull_ = glIsEnabled(GL_CULL_FACE);
  srgb_ = srgbControl_ ? glIsEnabled(GL_FRAMEBUFFER_SRGB_EXT) : GL_FALSE;
  glGetBooleanv(GL_COLOR_WRITEMASK, colorMask_);
  glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb_);
  glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb_);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha_);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha_);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEqRgb_);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEqAlpha_);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor_);
}

GlStateGuard::~GlStateGuard() {
  glUseProgram(static_cast<GLuint>(program_));
  glBindVertexArray(static_cast<GLuint>(vao_));
  glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer_));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture0_));
  glActiveTexture(static_cast<GLenum>(activeTexture_));
  glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFb_));
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFb_));
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpackBuffer_));
  glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment_);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLength_);
  glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
  glScissor(scissorBox_[0], scissorBox_[1], scissorBox_[2], scissorBox_[3]);
  if (scissor_) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
  if (blend_) glEnable(GL_BLEND); else glDisable(GL_BLEND);
  if (depth_) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  if (cull_) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
  if (srgbControl_) {
    if (srgb_) glEnable(GL_FRAMEBUFFER_SRGB_EXT); else glDisable(GL_FRAMEBUFFER_SRGB_EXT);
  }
  glColorMask(colorMask_[0], colorMask_[1], colorMask_[2], colorMask_[3]);
  glBlendFuncSeparate(static_cast<GLenum>(blendSrcRgb_), static_cast<GLenum>(blendDstRgb_),
                      static_cast<GLenum>(blendSrcAlpha_), static_cast<GLenum>(blendDstAlpha_));
  glBlendEquationSeparate(static_cast<GLenum>(blendEqRgb_), static_cast<GLenum>(blendEqAlpha_));
  glClearColor(clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]);
}

namespace {

const char* kVertex = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
uniform mat4 uMvp;
out vec2 vUv;
void main() {
  vUv = aUv;
  gl_Position = uMvp * vec4(aPos, 1.0);
})";

// Modus 0: Strahl (quer weich, zum Ende hin ausblendend), 1: Ring, 2: Punkt.
const char* kFragment = R"(#version 300 es
precision mediump float;
in vec2 vUv;
uniform vec4 uColor;
uniform int uMode;
out vec4 outColor;
void main() {
  float a;
  if (uMode == 0) {
    float across = 1.0 - smoothstep(0.2, 0.5, abs(vUv.y - 0.5));
    a = across * (1.0 - 0.7 * vUv.x);
  } else {
    float d = length(vUv - vec2(0.5)) * 2.0;
    if (uMode == 1) {
      a = smoothstep(0.45, 0.6, d) * (1.0 - smoothstep(0.85, 1.0, d));
    } else {
      a = 1.0 - smoothstep(0.7, 1.0, d);
    }
  }
  a *= uColor.a;
  outColor = vec4(uColor.rgb * a, a);
})";

GLuint compile(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint okay = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &okay);
  if (!okay) {
    char info[512] = {};
    glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
    XLOGE("gl", "Shader-Fehler: %s", info);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

GLuint link(const char* vertex, const char* fragment) {
  GLuint vs = compile(GL_VERTEX_SHADER, vertex);
  GLuint fs = compile(GL_FRAGMENT_SHADER, fragment);
  if (!vs || !fs) return 0;
  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (!linked) {
    char info[512] = {};
    glGetProgramInfoLog(program, sizeof(info), nullptr, info);
    XLOGE("gl", "Programm-Fehler: %s", info);
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

// Bildschirmfüllendes Dreieck ohne Vertexdaten; v = 0 unten (GL-Textur beginnt unten).
const char* kFilterVertex = R"(#version 300 es
out vec2 vUv;
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vUv = p;
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})";

const char* kFilterFragment = R"(#version 300 es
precision mediump float;
in vec2 vUv;
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform float uBrightness;  // Faktor 0.5 .. 1.5
uniform float uContrast;    // Faktor 0.5 .. 1.5
uniform float uSaturation;  // Faktor 0.0 .. 2.0
uniform float uSharp;       // 0 .. 1.5
uniform int uCrt;           // 0 normal, 1 Scanlines, 2 CRT
uniform vec2 uSize;         // Zielgröße in Pixeln (= Spielbild)
out vec4 outColor;
void main() {
  vec3 c = texture(uTex, vUv).rgb;
  if (uSharp > 0.0) {
    vec3 blur = (texture(uTex, vUv + vec2(uTexel.x, 0.0)).rgb + texture(uTex, vUv - vec2(uTexel.x, 0.0)).rgb +
                 texture(uTex, vUv + vec2(0.0, uTexel.y)).rgb + texture(uTex, vUv - vec2(0.0, uTexel.y)).rgb) * 0.25;
    c += (c - blur) * uSharp;
  }
  c *= uBrightness;
  c = (c - 0.5) * uContrast + 0.5;
  float luma = dot(c, vec3(0.299, 0.587, 0.114));
  c = mix(vec3(luma), c, uSaturation);
  if (uCrt > 0) {
    // Scanlines im 2-Pixel-Raster (Zeile hell, Zeile dunkel; War1gus: genau eine Zeile je Spielpixel).
    // Ein weicher Verlauf ginge bei 2 Pixeln je Zeile verloren (beide Pixelmitten gleich hell).
    vec2 px = vUv * uSize;
    c *= mod(floor(px.y), 2.0) < 1.0 ? 1.0 : 0.62;
    c *= 1.18;
    if (uCrt > 1) {
      // Streifenmaske (Apertur-Gitter) dezent, damit kein Farbsaum entsteht
      float m = mod(floor(px.x), 3.0);
      c *= m < 1.0 ? vec3(1.06, 0.92, 0.92) : m < 2.0 ? vec3(0.92, 1.06, 0.92) : vec3(0.92, 0.92, 1.06);
      // Vignette (Skill xr-image-quality): Ränder abdunkeln, Mitte leicht anheben
      float v = pow(16.0 * vUv.x * vUv.y * (1.0 - vUv.x) * (1.0 - vUv.y), 0.3);
      c *= mix(0.72, 1.0, v) * 1.08;
    }
  }
  outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
})";

}  // namespace

bool FilterRenderer::init() {
  program_ = link(kFilterVertex, kFilterFragment);
  if (!program_) return false;
  texLoc_ = glGetUniformLocation(program_, "uTex");
  texelLoc_ = glGetUniformLocation(program_, "uTexel");
  brightLoc_ = glGetUniformLocation(program_, "uBrightness");
  contrastLoc_ = glGetUniformLocation(program_, "uContrast");
  satLoc_ = glGetUniformLocation(program_, "uSaturation");
  sharpLoc_ = glGetUniformLocation(program_, "uSharp");
  crtLoc_ = glGetUniformLocation(program_, "uCrt");
  sizeLoc_ = glGetUniformLocation(program_, "uSize");
  glGenVertexArrays(1, &vao_);
  return true;
}

void FilterRenderer::draw(GLuint texture, int width, int height, int brightness, int contrast, int saturation,
                          int sharpness, int crt, int outWidth, int outHeight) {
  glUseProgram(program_);
  glBindVertexArray(vao_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glUniform1i(texLoc_, 0);
  glUniform2f(texelLoc_, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
  glUniform1f(brightLoc_, 0.5f + static_cast<float>(brightness) / 100.0f);
  glUniform1f(contrastLoc_, 0.5f + static_cast<float>(contrast) / 100.0f);
  glUniform1f(satLoc_, static_cast<float>(saturation) / 50.0f);
  glUniform1f(sharpLoc_, static_cast<float>(sharpness) / 100.0f * 1.5f);
  glUniform1i(crtLoc_, crt);
  glUniform2f(sizeLoc_, static_cast<float>(outWidth), static_cast<float>(outHeight));
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindTexture(GL_TEXTURE_2D, 0);
}

bool BeamRenderer::init() {
  program_ = link(kVertex, kFragment);
  if (!program_) return false;
  mvpLoc_ = glGetUniformLocation(program_, "uMvp");
  colorLoc_ = glGetUniformLocation(program_, "uColor");
  modeLoc_ = glGetUniformLocation(program_, "uMode");

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, 4 * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        reinterpret_cast<const void*>(3 * sizeof(float)));
  return true;
}

void BeamRenderer::draw(const float* mvp, const float* vertices, const float color[4], Mode mode) {
  glUseProgram(program_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * 5 * sizeof(float), vertices);
  glUniformMatrix4fv(mvpLoc_, 1, GL_FALSE, mvp);
  glUniform4fv(colorLoc_, 1, color);
  glUniform1i(modeLoc_, static_cast<GLint>(mode));
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

}  // namespace xrs
