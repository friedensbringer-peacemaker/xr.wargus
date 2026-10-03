// Fenster-Geometrie der XR-Schicht, abhängigkeitsfrei (nur xr_math.h) und dadurch per
// Host-Test prüfbar (tests/test_geometry.cpp). Eine Pose speist Compositor und Treffer.
#pragma once

#include <algorithm>

#include "xr_math.h"

namespace xrs {

// Grenzen aus dem Lernstand der XR-Ports: Abstand 0,7–3,5 m, Breite 1–3,2 m und
// 0,55–1,8 × Abstand.
struct PanelLimits {
  static constexpr float kMinDistance = 0.7f;
  static constexpr float kMaxDistance = 3.5f;
  static constexpr float kMinWidth = 1.0f;
  static constexpr float kMaxWidth = 3.2f;
};

inline void clampPanelSize(float* width, float* distance) {
  *distance = std::clamp(*distance, PanelLimits::kMinDistance, PanelLimits::kMaxDistance);
  float lo = std::max(PanelLimits::kMinWidth, 0.55f * *distance);
  float hi = std::min(PanelLimits::kMaxWidth, 1.8f * *distance);
  *width = std::clamp(*width, lo, std::max(lo, hi));
}

// Fensterpose: im Abstand distance vor anchor in Richtung yaw, aufrecht.
inline XrPosef panelPoseFor(XrVector3f anchor, float yaw, float distance) {
  XrQuaternionf q = yawQuat(yaw);
  XrVector3f forward = rotate(q, {0.0f, 0.0f, -1.0f});
  return {q, add(anchor, scale(forward, distance))};
}

// Strahl (Aim-Pose, zeigt entlang −Z) gegen eine Fläche width×height mit Pose quad; liefert
// u,v (0..1 = innerhalb, u nach rechts, v nach unten) und den Trefferpunkt. false: parallel,
// von hinten oder Fläche hinter dem Strahl.
inline bool hitQuad(const XrPosef& aim, const XrPosef& quad, float width, float height, float* u, float* v,
                    XrVector3f* point) {
  XrVector3f normal = rotate(quad.orientation, {0.0f, 0.0f, 1.0f});
  XrVector3f dir = rotate(aim.orientation, {0.0f, 0.0f, -1.0f});
  float denom = dot(dir, normal);
  if (denom > -1e-4f) return false;  // parallel oder von hinten
  float t = dot(sub(quad.position, aim.position), normal) / denom;
  if (t <= 0.0f) return false;
  *point = add(aim.position, scale(dir, t));
  XrVector3f local = rotate(conjugate(quad.orientation), sub(*point, quad.position));
  *u = local.x / width + 0.5f;
  *v = 0.5f - local.y / height;
  return true;
}

}  // namespace xrs
