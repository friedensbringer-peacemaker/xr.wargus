// Parameter-Sweeps für Fensterpose, Strahltreffer und Vektor-/Quaternion-Hilfen (native/xr).
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "xr_geometry.h"
#include "xr_math.h"

using namespace xrs;

namespace {

int g_checks = 0, g_failures = 0;

void check(bool ok, const char* what, double a = 0, double b = 0) {
  ++g_checks;
  if (!ok) {
    ++g_failures;
    if (g_failures <= 20) std::printf("FEHLER: %s (%g / %g)\n", what, a, b);
  }
}

bool near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }
bool near(XrVector3f a, XrVector3f b, float eps) {
  return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}

// Orientierung, deren −Z-Achse in Richtung dir zeigt (Yaw, dann Pitch, wie eine Aim-Pose).
XrQuaternionf lookAlong(XrVector3f dir) {
  dir = normalize(dir);
  float yaw = std::atan2(-dir.x, -dir.z);
  float pitch = std::asin(std::fmax(-1.0f, std::fmin(1.0f, dir.y)));
  return multiply(yawQuat(yaw), pitchQuat(pitch));
}

void testMath() {
  const float pi = 3.14159265f;
  for (int i = -35; i <= 35; ++i) {
    float yaw = static_cast<float>(i) * pi / 36.0f;
    check(near(yawOf(yawQuat(yaw)), yaw, 1e-4f), "yawOf(yawQuat)", yawOf(yawQuat(yaw)), yaw);
    for (int j = -8; j <= 8; ++j) {
      float pitch = static_cast<float>(j) * pi / 18.0f * 0.95f;
      XrQuaternionf q = multiply(yawQuat(yaw), pitchQuat(pitch));
      XrVector3f v{0.3f, -1.2f, 2.5f};
      check(near(length(rotate(q, v)), length(v), 1e-4f), "rotate erhält Länge");
      check(near(rotate(conjugate(q), rotate(q, v)), v, 1e-4f), "conjugate kehrt um");
      // Yaw bleibt bei Pitch erhalten (Blickrichtung nicht senkrecht).
      check(near(yawOf(q), yaw, 1e-3f), "yawOf mit Pitch", yawOf(q), yaw);
      // lookAlong: −Z zeigt in die gewünschte Richtung.
      XrVector3f dir = rotate(q, {0.0f, 0.0f, -1.0f});
      check(near(rotate(lookAlong(dir), {0.0f, 0.0f, -1.0f}), dir, 1e-4f), "lookAlong");
      // viewMatrix: Kamera liegt im Ursprung, Blickrichtung = −Z.
      XrPosef pose{q, {1.5f, 1.6f, -0.4f}};
      Mat4 view = viewMatrix(pose);
      auto apply = [&](XrVector3f p) {
        return XrVector3f{view.m[0] * p.x + view.m[4] * p.y + view.m[8] * p.z + view.m[12],
                          view.m[1] * p.x + view.m[5] * p.y + view.m[9] * p.z + view.m[13],
                          view.m[2] * p.x + view.m[6] * p.y + view.m[10] * p.z + view.m[14]};
      };
      check(near(apply(pose.position), {0, 0, 0}, 1e-4f), "viewMatrix: Auge im Ursprung");
      check(near(apply(add(pose.position, dir)), {0, 0, -1}, 1e-4f), "viewMatrix: Blick entlang -Z");
    }
  }
}

void testClamp() {
  for (int d = 0; d <= 60; ++d) {
    for (int w = 0; w <= 60; ++w) {
      float distance = 0.1f * static_cast<float>(d);
      float width = 0.1f * static_cast<float>(w);
      clampPanelSize(&width, &distance);
      check(distance >= PanelLimits::kMinDistance - 1e-6f && distance <= PanelLimits::kMaxDistance + 1e-6f,
            "Abstand in Grenzen", distance);
      check(width >= PanelLimits::kMinWidth - 1e-6f && width <= PanelLimits::kMaxWidth + 1e-6f, "Breite in Grenzen",
            width);
      check(width >= 0.55f * distance - 1e-5f || width >= PanelLimits::kMaxWidth - 1e-6f, "Breite >= 0,55 x Abstand",
            width, distance);
      check(width <= 1.8f * distance + 1e-5f || width <= PanelLimits::kMinWidth + 1e-6f, "Breite <= 1,8 x Abstand",
            width, distance);
      // Idempotent
      float w2 = width, d2 = distance;
      clampPanelSize(&w2, &d2);
      check(w2 == width && d2 == distance, "clamp idempotent");
    }
  }
}

void testHits() {
  const float pi = 3.14159265f;
  const float aspects[] = {1.6f, 4.0f / 3.0f, 16.0f / 9.0f, 2.0f, 0.75f};
  const float distances[] = {0.7f, 1.2f, 1.8f, 2.8f, 3.5f};
  const float widths[] = {1.0f, 2.0f, 3.2f};
  const XrVector3f anchor{0.2f, 1.45f, -0.3f};
  int hits = 0, misses = 0;
  for (float aspect : aspects) {
    for (int yi = 0; yi < 7; ++yi) {
      float yaw = (static_cast<float>(yi) - 3.0f) * 50.0f * pi / 180.0f;
      for (float distance : distances) {
        for (float width : widths) {
          float height = width / aspect;
          XrPosef quad = panelPoseFor(anchor, yaw, distance);
          // Fenster steht im gewünschten Abstand und zeigt zum Anker.
          check(near(length(sub(quad.position, anchor)), distance, 1e-4f), "Fensterabstand");
          XrVector3f normal = rotate(quad.orientation, {0, 0, 1});
          check(dot(normal, sub(anchor, quad.position)) > 0.0f, "Fenster zeigt zum Betrachter");
          XrVector3f right = rotate(quad.orientation, {1, 0, 0});
          XrVector3f up = rotate(quad.orientation, {0, 1, 0});
          const XrVector3f origins[2] = {anchor, add(anchor, {0.25f, -0.35f, 0.1f})};
          for (const XrVector3f& origin : origins) {
            for (int ui = 0; ui < 5; ++ui) {
              for (int vi = 0; vi < 3; ++vi) {
                float u0 = 0.05f + 0.225f * static_cast<float>(ui);
                float v0 = 0.1f + 0.4f * static_cast<float>(vi);
                XrVector3f target =
                    add(quad.position, add(scale(right, (u0 - 0.5f) * width), scale(up, (0.5f - v0) * height)));
                XrPosef aim{lookAlong(sub(target, origin)), origin};
                float u = -1, v = -1;
                XrVector3f p{};
                bool hit = hitQuad(aim, quad, width, height, &u, &v, &p);
                check(hit, "Treffer erwartet");
                check(near(u, u0, 1e-3f), "u", u, u0);
                check(near(v, v0, 1e-3f), "v", v, v0);
                check(near(p, target, 2e-3f), "Trefferpunkt");
                ++hits;
                // Rückwärts: Strahl zeigt vom Fenster weg.
                XrPosef away{lookAlong(sub(origin, target)), origin};
                check(!hitQuad(away, quad, width, height, &u, &v, &p), "rückwärts kein Treffer");
                ++misses;
              }
            }
          }
          // Parallel zur Fläche: kein Treffer.
          XrPosef parallel{lookAlong(right), anchor};
          float u, v;
          XrVector3f p;
          check(!hitQuad(parallel, quad, width, height, &u, &v, &p), "parallel kein Treffer");
          // Von hinten auf das Fenster: kein Treffer.
          XrVector3f behind = add(quad.position, scale(normal, -1.0f));
          XrPosef back{lookAlong(sub(quad.position, behind)), behind};
          check(!hitQuad(back, quad, width, height, &u, &v, &p), "von hinten kein Treffer");
          // Daneben: Treffer der Ebene, aber u außerhalb 0..1.
          XrVector3f outside = add(quad.position, scale(right, width));
          XrPosef side{lookAlong(sub(outside, anchor)), anchor};
          check(hitQuad(side, quad, width, height, &u, &v, &p) && (u > 1.0f || u < 0.0f), "daneben: u außerhalb");
          misses += 3;
        }
      }
    }
  }
  std::printf("Treffer %d, Fehlschüsse %d\n", hits, misses);
}

}  // namespace

int main() {
  testMath();
  testClamp();
  testHits();
  std::printf("test_geometry: %d Prüfungen, %d Fehler\n", g_checks, g_failures);
  return g_failures ? 1 : 0;
}
