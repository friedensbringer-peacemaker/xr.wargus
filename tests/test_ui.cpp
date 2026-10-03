// VR-Anzeigen (native/xr): Menü (alle Reiter, Mensch/Ork), Tastatur, Ladeanzeige, Theme und
// Einstellungen. Läuft am PC (llvm-mingw) und auf der Quest (ARM64, echte Gerätebibliotheken).
//
//   test_ui <data.Wargus> <Arbeitsordner> [PNG-Ausgabeordner]
//   XRS_GAME=war1gus test_ui <data.War1gus> …   (zweite App xr.wargus1, native/xr/xr_game.h)
//
// Prüft: Spielgrafiken/-schrift geladen, jede Schaltfläche liegt im Bild, überlappt keine
// andere, ist über hit() in ihrer Mitte erreichbar, ihre Beschriftung passt hinein; gesperrte
// Schaltflächen sind nicht klickbar; Tastatur schaltet Umschalt nur für ein Zeichen;
// Einstellungen überstehen Speichern/Laden; Ladeanzeige zeichnet alle Schritte.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "xr_game.h"
#include "xr_keyboard.h"
#include "xr_loader.h"
#include "xr_log.h"
#include "xr_menu.h"
#include "xr_settings.h"
#include "xr_theme.h"

// xr_log.cpp ist Android-spezifisch (Logdatei, logcat): hier schlicht, nur Warnungen/Fehler.
namespace xrs {
void logf(LogLevel level, const char* area, const char* fmt, ...) {
  if (level < LogLevel::kWarn) return;
  va_list args;
  va_start(args, fmt);
  std::fprintf(stderr, "%s: ", area);
  std::vfprintf(stderr, fmt, args);
  std::fputc('\n', stderr);
  va_end(args);
}
}  // namespace xrs

using namespace xrs;

namespace {

int g_checks = 0, g_failures = 0;

void check(bool ok, const std::string& what) {
  ++g_checks;
  if (!ok) {
    ++g_failures;
    if (g_failures <= 30) std::printf("FEHLER: %s\n", what.c_str());
  }
}

void setEnv(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

std::string g_out;

void save(const uint8_t* pixels, int w, int h, const std::string& name) {
  if (g_out.empty()) return;
  SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uint8_t*>(pixels), w, h, 32, w * 4,
                                                      SDL_PIXELFORMAT_RGBA32);
  check(s && IMG_SavePNG(s, (g_out + "/" + name).c_str()) == 0, "PNG speichern " + name);
  SDL_FreeSurface(s);
}

struct Box {
  int x, y, w, h;
};

// Gemeinsame Layout-Prüfung für Menü und Tastatur.
template <typename Panel, typename LabelFn, typename GrayFn>
void checkLayout(Panel& panel, int canvasW, int canvasH, const std::string& name, LabelFn label, GrayFn grayed) {
  const Theme& theme = Theme::get();
  int n = panel.count();
  check(n > 0, name + ": Schaltflächen vorhanden");
  std::vector<Box> boxes(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    Box& b = boxes[static_cast<size_t>(i)];
    panel.rect(i, &b.x, &b.y, &b.w, &b.h);
    std::string id = name + " #" + std::to_string(i) + " \"" + label(i) + "\"";
    check(b.x >= 0 && b.y >= 0 && b.x + b.w <= canvasW && b.y + b.h <= canvasH, id + " liegt im Bild");
    check(b.w >= 40 && b.h >= 30, id + " groß genug zum Treffen");
    float u = (static_cast<float>(b.x) + 0.5f * static_cast<float>(b.w)) / static_cast<float>(canvasW);
    float v = (static_cast<float>(b.y) + 0.5f * static_cast<float>(b.h)) / static_cast<float>(canvasH);
    int hit = panel.hit(u, v);
    if (grayed(i)) {
      check(hit == -1, id + " gesperrt: nicht klickbar");
    } else {
      check(hit == i, id + " in der Mitte erreichbar");
    }
    int textW = theme.textWidth(label(i), 1);
    // Wargus: passt in Stufe 1; breitere Schriften (War1gus) dürfen bis 80 % gestaucht werden.
    check(theme.fits(label(i), b.w - 8), id + " Beschriftung passt (" + std::to_string(textW) + " px)");
    if (!std::strcmp(currentGame().id, "wargus")) {
      check(textW <= b.w - 8, id + " Wargus: Beschriftung passt ungestaucht (" + std::to_string(textW) + " px)");
    }
  }
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      const Box& a = boxes[static_cast<size_t>(i)];
      const Box& b = boxes[static_cast<size_t>(j)];
      bool overlap = a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
      check(!overlap, name + ": #" + std::to_string(i) + " und #" + std::to_string(j) + " überlappen nicht");
    }
  }
  // Außerhalb aller Schaltflächen: kein Treffer.
  check(panel.hit(0.001f, 0.001f) == -1, name + ": Ecke ohne Treffer");
}

void testMenu() {
  Settings s;
  for (int orc = 0; orc < 2; ++orc) {
    Theme::get().setOrc(orc != 0);
    check(Theme::get().orc() == (orc != 0), "Menüstil umgeschaltet");
    // movie: 0 = kein Film, 1 = Film läuft, 2 = kein Film, Beenden scharf
    for (int movie = 0; movie < 3; ++movie) {
      for (int page = kPageQuick; page <= kPageDisplay; ++page) {
        VrMenu menu;
        menu.setPage(page);
        const bool armed = movie == 2;
        menu.update(s, GroupMode::kStore, "Spiel 30 fps - XR 72 Hz - Fenster 2.0 m, 1.8 m entfernt", movie != 0,
                    movie == 1, armed);
        std::string name = std::string("Menü ") + (orc ? "Ork" : "Mensch") + " Reiter " + std::to_string(page) +
                           (movie == 1 ? " (Film)" : armed ? " (Beenden scharf)" : "");
        checkLayout(menu, VrMenu::kWidth, VrMenu::kHeight, name, [&](int i) { return menu.label(i); },
                    [&](int i) { return menu.grayed(i); });
        // Reiter führen auf ihre Seite; „Film überspringen“ nur während eines Films klickbar; jede
        // Aktion (außer Gruppen/Regler mit Wert) kommt auf allen Seiten zusammen genau einmal vor.
        bool foundSkip = false, foundQuit = false, foundBack = false, foundClose = false, foundSettings = false;
        int large = 0, modes = 0, groups = 0, activeMode = -1;
        for (int i = 0; i < menu.count(); ++i) {
          MenuHit h = menu.action(i);
          if (h.action == MenuAction::kSkipMovie) {
            foundSkip = true;
            check(menu.grayed(i) == (movie != 1), name + ": Film überspringen nur bei Film");
          }
          if (h.action == MenuAction::kQuit) {
            foundQuit = true;
            check(menu.label(i) == (armed ? "Sicher? Nochmal klicken" : "Beenden"),
                  name + ": Beenden-Beschriftung zeigt den Scharf-Zustand");
          }
          if (h.action == MenuAction::kBack) foundBack = true;
          if (h.action == MenuAction::kClose) foundClose = true;
          if (h.action == MenuAction::kOpenSettings) foundSettings = true;
          if (h.action == MenuAction::kGroup) {
            check(h.value >= 1 && h.value <= 9 && menu.label(i) == std::to_string(h.value), name + ": Gruppe 1–9");
            check(menu.strip(i), name + ": Gruppe in der Gruppenleiste");
            ++groups;
          }
          if (h.action == MenuAction::kGroupMode) {
            ++modes;
            if (menu.selected(i)) activeMode = h.value;
          }
          if (!menu.strip(i)) ++large;
        }
        // P5: Schnellmenü ≤ 8 große Knöpfe mit Beenden/Einstellungen; jede Seite endet mit „Schließen“,
        // Einstellungsseiten zusätzlich mit „Zurück“.
        if (page == kPageQuick) {
          check(large <= 8, name + ": Schnellmenü höchstens 8 große Knöpfe");
          check(foundQuit && foundSettings && foundClose && !foundBack, name + ": Beenden, Einstellungen, Schließen");
          // Gruppenleiste direkt im Schnellmenü: 3 Modi (genau einer aktiv = übergebener Modus), Gruppen 1–9
          check(modes == 3 && groups == 9, name + ": Gruppenleiste 3 Modi + 9 Gruppen");
          check(activeMode == static_cast<int>(GroupMode::kStore), name + ": aktiver Gruppenmodus hervorgehoben");
        } else {
          check(modes == 0 && groups == 0, name + ": Gruppen nur im Schnellmenü (jeder Eintrag einmal)");
          check(!foundQuit && !foundSettings, name + ": Beenden nur im Schnellmenü");
          check(foundBack && foundClose, name + ": Zurück und Schließen vorhanden");
        }
        if (page == kPageGame) check(foundSkip, name + ": Film überspringen vorhanden");
        // P2: Stick-Fokus startet auf einer Nicht-Reiter-Schaltfläche, bewegt sich und bleibt am Rand stehen.
        {
          int f = menu.navigate(-1, 0, 0);
          check(f >= 0 && !menu.grayed(f), name + ": Startfokus klickbar");
          int down = menu.navigate(f, 0, 1);
          int fx, fy, fw, fh, dx, dy, dw, dh;
          menu.rect(f, &fx, &fy, &fw, &fh);
          menu.rect(down, &dx, &dy, &dw, &dh);
          check(down != f && dy > fy, name + ": Stick runter geht eine Zeile tiefer");
          int last = f;
          for (int k = 0; k < 40; ++k) last = menu.navigate(last, 0, 1);
          check(menu.navigate(last, 0, 1) == last, name + ": unten bleibt der Fokus stehen");
          check(!menu.grayed(last), name + ": Fokus nie auf gesperrter Schaltfläche");
          MenuAction lastAction = menu.action(last).action;
          if (page == kPageQuick) {
            check(lastAction == MenuAction::kGroup, name + ": unterste Zeile sind die Gruppen 1–9");
            // Gruppen sind per Stick seitlich durchlaufbar (1 → 9)
            int g = last;
            int start = -1;
            for (int k = 0; k < 12; ++k) g = menu.navigate(g, -1, 0);
            start = menu.action(g).value;
            for (int k = 0; k < 12; ++k) g = menu.navigate(g, 1, 0);
            check(start == 1 && menu.action(g).value == 9, name + ": Stick links/rechts über 1–9");
          } else {
            check(lastAction == MenuAction::kClose || lastAction == MenuAction::kBack,
                  name + ": unterste Zeile ist Zurück/Schließen");
          }
          if (page != kPageQuick) {
            int right = menu.navigate(f, 1, 0);
            int rx, ry, rw, rh;
            menu.rect(right, &rx, &ry, &rw, &rh);
            check(right != f && rx > fx && ry == fy, name + ": Stick rechts bleibt in der Zeile");
          }
        }
        const uint8_t* px = menu.pixels();
        uint32_t v1 = menu.version();
        menu.setHover(3);
        menu.pixels();
        check(px != nullptr && menu.version() == v1 + 1, name + ": Hover zeichnet neu");
        menu.pixels();
        check(menu.version() == v1 + 1, name + ": ohne Änderung kein Neuzeichnen");
        if (movie == 0 || (armed && page == kPageQuick)) {
          save(menu.pixels(), VrMenu::kWidth, VrMenu::kHeight,
               std::string("menu-") + (orc ? "ork" : "mensch") + "-" + std::to_string(page) +
                   (armed ? "-beenden-scharf" : "") + ".png");
        }
      }
    }
  }
  Theme::get().setOrc(false);
}

void testKeyboard() {
  KeyboardPanel k;
  checkLayout(k, KeyboardPanel::kWidth, KeyboardPanel::kHeight, "Tastatur", [&](int i) { return k.label(i); },
              [](int) { return false; });
  int shift = -1, a = -1;
  for (int i = 0; i < k.count(); ++i) {
    if (k.label(i) == "Umschalt") shift = i;
    if (k.label(i) == "a") a = i;
  }
  check(shift >= 0 && a >= 0, "Tastatur: Umschalt und a vorhanden");
  if (shift >= 0 && a >= 0) {
    check(k.press(a).text == "a", "Tastatur: a klein");
    check(k.press(shift).kind == KeyKind::kShift, "Tastatur: Umschalt");
    check(k.press(a).text == "A", "Tastatur: A nach Umschalt");
    check(k.press(a).text == "a", "Tastatur: Umschalt gilt nur ein Zeichen");
  }
  save(k.pixels(), KeyboardPanel::kWidth, KeyboardPanel::kHeight, "tastatur.png");
}

void testLoader() {
  LoaderPanel l;
  LoaderState st;
  uint32_t last = 0;
  for (int step = 0; step <= 4; ++step) {
    st.step = static_cast<LoadStep>(step);
    st.seconds += 1.0f;
    check(l.pixels(st) != nullptr && l.version() > last, "Ladeanzeige Schritt " + std::to_string(step));
    last = l.version();
  }
  st.dataMissing = true;
  check(l.pixels(st) != nullptr && l.version() > last, "Ladeanzeige Daten fehlen");
  save(l.pixels(st), LoaderPanel::kWidth, LoaderPanel::kHeight, "daten-fehlen.png");
}

void testSettings(const std::string& workDir) {
  setEnv("XRS_USER_DIR", workDir.c_str());
  std::remove(settingsPath().c_str());
  Settings d = loadSettings();
  check(d.pointerHand == 1 && !d.autoSkipMovies && !d.menuOrc && d.image.neutral(), "Standardwerte ohne Datei");
  Settings s;
  s.pointerHand = 0;
  s.width = 2.6f;
  s.distance = 2.2f;
  s.beam = false;
  s.target = TargetStyle::kDot;
  s.logInput = true;
  s.passthrough = true;
  s.autoSkipMovies = true;
  s.menuOrc = true;
  s.gameCursor = false;
  s.image.brightness = 70;
  s.image.contrast = 30;
  s.image.saturation = 80;
  s.image.sharpness = 40;
  s.image.crt = 2;
  saveSettings(s);
  Settings r = loadSettings();
  check(r.pointerHand == 0 && std::fabs(r.width - 2.6f) < 1e-3f && std::fabs(r.distance - 2.2f) < 1e-3f && !r.beam &&
            r.target == TargetStyle::kDot && r.logInput && r.passthrough && r.autoSkipMovies && r.menuOrc &&
            !r.gameCursor && r.image.brightness == 70 && r.image.contrast == 30 && r.image.saturation == 80 &&
            r.image.sharpness == 40 && r.image.crt == 2,
        "Einstellungen überstehen Speichern/Laden");
  // Kaputte Werte werden begrenzt statt übernommen.
  if (FILE* f = std::fopen(settingsPath().c_str(), "w")) {
    std::fputs("brightness=250\ncontrast=-40\ntarget=7\npointer_hand=5\ncrt=9\nunbekannt=1\n", f);
    std::fclose(f);
  }
  Settings b = loadSettings();
  check(b.image.brightness == 100 && b.image.contrast == 0, "Regler auf 0–100 begrenzt");
  check(static_cast<int>(b.target) >= 0 && static_cast<int>(b.target) <= 2, "Zielpunkt gültig");
  check(b.pointerHand == 1, "Zeigerhand gültig");
  check(b.image.crt >= 0 && b.image.crt <= 2, "Bildmodus gültig");
  std::remove(settingsPath().c_str());
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "Aufruf: [XRS_GAME=war1gus] %s <data.Wargus|data.War1gus> <Arbeitsordner> [PNG-Ausgabe]\n",
                 argv[0]);
    return 2;
  }
  setEnv("XRS_DATA_DIR", argv[1]);
  if (argc > 3) g_out = argv[3];
  SDL_SetMainReady();

  const GameProfile& game = currentGame();
  std::printf("test_ui: Spielpaket %s (%s)\n", game.engineName, game.appId);
  Theme::get().setOrc(false);
  check(Theme::get().assets(), "Spielgrafiken (Panel) aus den Spieldaten geladen");
  // Wargus hat Knopfgrafiken, War1gus nicht (dort gezeichneter Ersatz, Profil ohne Pfade).
  check(Theme::get().buttonAssets() == (*game.buttonImages[0] != '\0'), "Schaltflächen: Spielgrafik bzw. Ersatz");
  check(Theme::get().gameFont(), "Menüschrift aus den Spieldaten geladen");
  check(Theme::get().textWidth("Weiterspielen", 2) == 2 * Theme::get().textWidth("Weiterspielen", 1),
        "Schriftbreite skaliert linear");
  testMenu();
  testKeyboard();
  testLoader();
  testSettings(argv[2]);
  std::printf("test_ui: %d Prüfungen, %d Fehler\n", g_checks, g_failures);
  return g_failures ? 1 : 0;
}
