// Eingabe-Bausteine (native/xr/xr_input.h): Hysterese, Re-Arm, Kurz/Lang ohne Doppelaktion,
// Deadzone, bildratenunabhängige Glättung – jeweils über die Quest-Bildraten 36/72/90/120 Hz.
#include <cmath>
#include <cstdio>

#include "xr_input.h"
#include "xr_menu_model.h"

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

void testAnalog() {
  AnalogButton b;
  check(b.update(0.3f) == 0 && !b.down(), "unter Schwelle kein Druck");
  check(b.update(0.6f) == 1 && b.down(), "Druck über 0,55");
  check(b.update(0.5f) == 0 && b.down(), "Hysterese hält");
  check(b.update(0.4f) == 0 && b.down(), "Hysterese hält bis 0,35");
  check(b.update(0.3f) == -1 && !b.down(), "Loslassen unter 0,35");
  // Re-Arm: nach reset() zählt ein noch gehaltener Trigger nicht als neuer Druck.
  check(b.update(0.9f) == 1, "Druck");
  b.reset();
  check(!b.down(), "reset löst");
  for (int i = 0; i < 50; ++i) check(b.update(0.9f) == 0, "gehalten nach reset: kein Druck");
  check(b.update(0.2f) == 0, "loslassen re-armt still");
  check(b.update(0.8f) == 1, "danach wieder Druck");
  // Rauschen um eine Schwelle erzeugt keine Mehrfachklicks.
  AnalogButton n;
  int presses = 0;
  for (int i = 0; i < 1000; ++i) {
    float v = 0.45f + 0.09f * std::sin(static_cast<float>(i));  // 0,36 … 0,54
    if (n.update(v) == 1) ++presses;
  }
  check(presses == 0, "Rauschen zwischen den Schwellen löst nie aus", presses);
}

void testPress() {
  const float rates[] = {36.0f, 72.0f, 90.0f, 120.0f};
  for (float hz : rates) {
    float dt = 1.0f / hz;
    for (int ms = 20; ms <= 2000; ms += 20) {
      PressButton p;
      int shorts = 0, longs = 0;
      float held = static_cast<float>(ms) / 1000.0f;
      int frames = static_cast<int>(std::round(held * hz));
      for (int f = 0; f < frames; ++f) {
        PressButton::Event e = p.update(true, dt);
        shorts += e == PressButton::kShort;
        longs += e == PressButton::kLong;
      }
      for (int f = 0; f < 5; ++f) {
        PressButton::Event e = p.update(false, dt);
        shorts += e == PressButton::kShort;
        longs += e == PressButton::kLong;
      }
      check(shorts + longs <= 1, "nie Kurz und Lang zugleich", shorts, longs);
      if (frames == 0) continue;
      // Gehaltene Zeit (erster Frame zählt als Beginn): lang ab 0,6 s.
      float measured = static_cast<float>(frames - 1) * dt;
      if (measured >= 0.6f + dt) check(longs == 1 && shorts == 0, "lang erkannt", measured, hz);
      if (measured < 0.6f - dt) check(shorts == 1 && longs == 0, "kurz erkannt", measured, hz);
    }
  }
  // Re-Arm nach reset(): gehaltene Taste löst nichts aus.
  PressButton r;
  r.update(true, 0.01f);
  r.reset();
  int events = 0;
  for (int i = 0; i < 200; ++i) events += r.update(true, 0.01f) != PressButton::kNone;
  events += r.update(false, 0.01f) != PressButton::kNone;
  check(events == 0, "nach reset keine Aktion bis Loslassen", events);
}

void testDeadzone() {
  float last = 0.0f;
  for (int i = 0; i <= 100; ++i) {
    float v = static_cast<float>(i) / 100.0f;
    float d = applyDeadzone(v);
    check(d == -applyDeadzone(-v), "Deadzone symmetrisch");
    if (v < 0.25f) check(d == 0.0f, "innerhalb Deadzone 0", v, d);
    check(d >= last, "monoton", d, last);
    check(d >= 0.0f && d <= 1.0f, "Bereich 0..1", d);
    last = d;
  }
  check(std::fabs(applyDeadzone(1.0f) - 1.0f) < 1e-6f, "Vollausschlag = 1");
}

void testSmoothing() {
  // Nach 0,2 s soll die Glättung bei allen Bildraten gleich weit sein.
  const float rates[] = {36.0f, 72.0f, 90.0f, 120.0f};
  float results[4];
  for (int r = 0; r < 4; ++r) {
    float dt = 1.0f / rates[r];
    float x = 0.0f;
    int frames = static_cast<int>(std::round(0.2f * rates[r]));
    for (int f = 0; f < frames; ++f) x += (1.0f - x) * smoothingAlpha(dt, 0.025f);
    results[r] = x;
  }
  for (int r = 1; r < 4; ++r) check(std::fabs(results[r] - results[0]) < 1e-3f, "Glättung bildratenunabhängig",
                                    results[r], results[0]);
  check(results[0] > 0.99f, "Glättung konvergiert", results[0]);
}

void testConfirmAndVisibility() {
  const float rates[] = {36.0f, 72.0f, 90.0f, 120.0f};
  for (float hz : rates) {
    const float dt = 1.0f / hz;
    // Zweiter Klick innerhalb 3 s bestätigt.
    ConfirmAction c;
    check(c.click() == ConfirmAction::kArmed && c.armed(), "erster Klick schaltet scharf");
    for (float t = 0.0f; t < 2.9f; t += dt) check(!c.update(dt) && c.armed(), "bleibt bis 3 s scharf");
    check(c.click() == ConfirmAction::kConfirmed && !c.armed(), "zweiter Klick bestätigt");
    // Ohne zweiten Klick fällt der Knopf nach 3 s genau einmal zurück.
    ConfirmAction f;
    f.click();
    int fallbacks = 0;
    for (float t = 0.0f; t < 4.0f; t += dt) fallbacks += f.update(dt) ? 1 : 0;
    check(fallbacks == 1 && !f.armed(), "Rückfall nach 3 s genau einmal", fallbacks, hz);
    check(f.click() == ConfirmAction::kArmed, "nach Rückfall wieder erster Klick");
  }
  ConfirmAction m;
  m.click();
  m.cancel();
  check(!m.armed() && m.click() == ConfirmAction::kArmed, "Menü schließen entschärft");
  ConfirmAction n;
  n.click();
  check(!n.update(std::nanf("")) && n.armed(), "NaN-dt zählt nicht");
  // Zeiger-Sichtbarkeit: im offenen Menü Strahl und Zielpunkt immer.
  for (int bits = 0; bits < 8; ++bits) {
    PointerVisibility v;
    v.ray = bits & 1;
    v.marker = bits & 2;
    v.gameCursor = bits & 4;
    check(v.showRay(true) && v.showMarker(true), "im Menü Strahl und Zielpunkt sichtbar");
    check(v.showRay(false) == v.ray && v.showMarker(false) == v.marker, "außerhalb nach Schalter");
    check(v.showGameCursor(false) == v.gameCursor && !v.showGameCursor(true), "Spiel-Mauszeiger nach Schalter");
  }
}

// P2: Dauerdruck-Wiederholung (Flanke sofort, nach 0,45 s alle 0,12 s) und Stick-Richtung.
void testRepeat() {
  const float rates[] = {36.0f, 72.0f, 90.0f, 120.0f};
  for (float hz : rates) {
    const float dt = 1.0f / hz;
    HoldRepeat r;
    int fires = 0;
    for (float t = 0.0f; t < 1.0f; t += dt) fires += r.update(true, dt);
    // 1 (Flanke) + Wiederholungen ab 0,45 s im Abstand 0,12 s: 0,45 0,57 0,69 0,81 0,93 → 6 (± Rasterung)
    check(fires >= 5 && fires <= 7, "Wiederholung in 1 s", fires, hz);
    check(r.update(false, dt) == 0, "Loslassen löst nicht aus");
    check(r.update(true, dt) == 1, "neue Flanke löst sofort aus");
    HoldRepeat held;
    held.reset();
    check(held.update(true, dt) == 0, "nach reset gehalten: nichts");
    held.update(false, dt);
    check(held.update(true, dt) == 1, "nach Loslassen wieder scharf");
    // Ein riesiger Frame-Sprung löst nicht unbegrenzt oft aus.
    HoldRepeat jump;
    jump.update(true, dt);
    check(jump.update(true, 5.0f) <= 4, "Frame-Sprung begrenzt");
  }
  int dx, dy;
  stickDirection(0.1f, 0.2f, &dx, &dy);
  check(dx == 0 && dy == 0, "Stick in Ruhe");
  stickDirection(0.0f, 0.9f, &dx, &dy);
  check(dx == 0 && dy == -1, "Stick hoch = Zeile höher");
  stickDirection(0.0f, -0.9f, &dx, &dy);
  check(dx == 0 && dy == 1, "Stick runter = Zeile tiefer");
  stickDirection(0.8f, 0.7f, &dx, &dy);
  check(dx == 1 && dy == 0, "dominante Achse gewinnt");
  stickDirection(-0.9f, 0.1f, &dx, &dy);
  check(dx == -1 && dy == 0, "Stick links");
}

// Karte bewegen: Stick-Tempo steigt je Stufe, klemmt 1–10; Kennlinie quadratisch mit Vorzeichen; Ziehfaktoren.
void testMapMove() {
  float last = 0.0f;
  for (int level = 1; level <= 10; ++level) {
    const float v = mapmove::stickViewsPerSecond(level);
    check(v > last, "Scrolltempo steigt", v, last);
    last = v;
  }
  check(std::fabs(mapmove::stickViewsPerSecond(5) - 0.3427f) < 1e-3f, "Stufe 5 ~ 0,34 Fensterbreiten/s");
  check(mapmove::stickViewsPerSecond(0) == mapmove::stickViewsPerSecond(1) &&
            mapmove::stickViewsPerSecond(99) == mapmove::stickViewsPerSecond(10),
        "Scrolltempo geklemmt");
  check(mapmove::stickCurve(0.5f) == 0.25f && mapmove::stickCurve(-0.5f) == -0.25f && mapmove::stickCurve(1.0f) == 1.0f,
        "Stick-Kennlinie");
  check(mapmove::dragFactor(0) == 1.0f && mapmove::dragFactor(3) == 3.0f && mapmove::dragFactor(9) == 3.0f &&
            mapmove::dragFactor(-1) == 1.0f,
        "Ziehfaktor");
}

// P3 Zeiger-Versatz: Raster 0,25 %, ±15 %, NaN -> 0, UV-Verschiebung am Rand geklemmt, Anzeige.
void testPointerOffset() {
  check(ptroff::clampPercent(0.3f) == 0.25f, "Raster 0,25", ptroff::clampPercent(0.3f));
  check(ptroff::clampPercent(99.0f) == 15.0f && ptroff::clampPercent(-99.0f) == -15.0f, "±15 % geklemmt");
  check(ptroff::clampPercent(std::nanf("")) == 0.0f, "NaN -> 0");
  check(ptroff::clampPercent(INFINITY) == 0.0f, "Inf -> 0");
  check(!std::signbit(ptroff::clampPercent(-0.1f)), "kein -0");
  float p = 0.0f;
  for (int i = 0; i < 100; ++i) p = ptroff::stepPercent(p, 1);
  check(p == 15.0f, "Dauerdruck › endet bei +15 %", p);
  for (int i = 0; i < 200; ++i) p = ptroff::stepPercent(p, -1);
  check(p == -15.0f, "Dauerdruck ‹ endet bei −15 %", p);
  check(std::fabs(ptroff::apply(0.5f, 1.25f) - 0.5125f) < 1e-6f, "UV + 1,25 %");
  check(std::fabs(ptroff::apply(0.5f, -0.5f) - 0.495f) < 1e-6f, "UV − 0,5 %");
  check(ptroff::apply(0.99f, 15.0f) == 1.0f && ptroff::apply(0.01f, -15.0f) == 0.0f, "am Bildrand geklemmt");
  check(ptroff::apply(0.3f, 0.0f) == 0.3f, "0 % ändert nichts");
  check(ptroff::format(0.0f) == "0", "Anzeige 0");
  check(ptroff::format(1.25f) == "+1,25 %", "Anzeige +1,25 %");
  check(ptroff::format(-0.5f) == "−0,50 %", "Anzeige −0,50 %");
  check(ptroff::format(std::nanf("")) == "0", "Anzeige NaN");
}

}  // namespace

int main() {
  testMapMove();
  testPointerOffset();
  testRepeat();
  testAnalog();
  testConfirmAndVisibility();
  testPress();
  testDeadzone();
  testSmoothing();
  std::printf("test_input: %d Prüfungen, %d Fehler\n", g_checks, g_failures);
  return g_failures ? 1 : 0;
}
