// Minimale Vektor-/Quaternion-Hilfen für Pose und Strahltreffer.
#pragma once

#include <openxr/openxr.h>

#include <cmath>

namespace xrs {

inline XrVector3f add(XrVector3f a, XrVector3f b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline XrVector3f sub(XrVector3f a, XrVector3f b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline XrVector3f scale(XrVector3f a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(XrVector3f a, XrVector3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline XrVector3f cross(XrVector3f a, XrVector3f b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Vektor mit Quaternion drehen.
inline XrVector3f rotate(XrQuaternionf q, XrVector3f v) {
  XrVector3f u{q.x, q.y, q.z};
  XrVector3f t = scale(cross(u, v), 2.0f);
  return add(add(v, scale(t, q.w)), cross(u, t));
}

inline XrQuaternionf conjugate(XrQuaternionf q) { return {-q.x, -q.y, -q.z, q.w}; }

// Drehung um die Hochachse (Y).
inline XrQuaternionf yawQuat(float yaw) {
  return {0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f)};
}

inline XrQuaternionf multiply(XrQuaternionf a, XrQuaternionf b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

// Drehung um die Querachse (X), positiv = Oberkante zum Betrachter.
inline XrQuaternionf pitchQuat(float pitch) {
  return {std::sin(pitch * 0.5f), 0.0f, 0.0f, std::cos(pitch * 0.5f)};
}

// Yaw (Drehung um Y) einer Orientierung aus deren Blickrichtung (-Z).
inline float yawOf(XrQuaternionf q) {
  XrVector3f fwd = rotate(q, {0.0f, 0.0f, -1.0f});
  return std::atan2(-fwd.x, -fwd.z);
}

inline float length(XrVector3f a) { return std::sqrt(dot(a, a)); }
inline XrVector3f normalize(XrVector3f a) {
  float l = length(a);
  return l > 1e-6f ? scale(a, 1.0f / l) : XrVector3f{0.0f, 0.0f, -1.0f};
}

// 4×4-Matrizen spaltenweise (m[Spalte * 4 + Zeile]), wie GL sie erwartet.
struct Mat4 {
  float m[16];
};

inline Mat4 multiply(const Mat4& a, const Mat4& b) {
  Mat4 r{};
  for (int c = 0; c < 4; ++c) {
    for (int row = 0; row < 4; ++row) {
      float s = 0.0f;
      for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
      r.m[c * 4 + row] = s;
    }
  }
  return r;
}

// Asymmetrische Projektion aus dem OpenXR-Sichtfeld (GL-Clipraum −1..1).
inline Mat4 projection(const XrFovf& fov, float nearZ, float farZ) {
  float l = std::tan(fov.angleLeft), r = std::tan(fov.angleRight);
  float u = std::tan(fov.angleUp), d = std::tan(fov.angleDown);
  float w = r - l, h = u - d;
  Mat4 p{};
  p.m[0] = 2.0f / w;
  p.m[5] = 2.0f / h;
  p.m[8] = (r + l) / w;
  p.m[9] = (u + d) / h;
  p.m[10] = -(farZ + nearZ) / (farZ - nearZ);
  p.m[11] = -1.0f;
  p.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
  return p;
}

// Inverse einer Pose (Welt → Auge).
inline Mat4 viewMatrix(const XrPosef& pose) {
  XrVector3f axis[3] = {rotate(pose.orientation, {1.0f, 0.0f, 0.0f}),
                        rotate(pose.orientation, {0.0f, 1.0f, 0.0f}),
                        rotate(pose.orientation, {0.0f, 0.0f, 1.0f})};
  Mat4 v{};
  for (int row = 0; row < 3; ++row) {
    v.m[0 * 4 + row] = axis[row].x;
    v.m[1 * 4 + row] = axis[row].y;
    v.m[2 * 4 + row] = axis[row].z;
    v.m[3 * 4 + row] = -dot(axis[row], pose.position);
  }
  v.m[15] = 1.0f;
  return v;
}

}  // namespace xrs
