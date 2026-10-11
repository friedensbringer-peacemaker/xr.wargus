// VR-Menü auf der linken Menütaste, gestaltet wie die Wargus-Spielmenüs (Steinpanel,
// Schaltflächen und Schrift aus den eigenen Spieldaten, siehe xr_theme.h). Pflichtumfang nach Skill
// xr-vr-menu Abschnitt 0:
//   P5 zwei Stufen – Schnellmenü (≤ 8 große Knöpfe: Weiterspielen, Fenster zentrieren, Passthrough,
//      Spielmenü, Tastatur, Einstellungen, Beenden, Schließen; darunter die Gruppenleiste: Modus
//      Wählen/Speichern/Ergänzen und schmale Knöpfe 1–9) und Einstellungen mit drei Reitern
//      (Spiel, Fenster, Anzeige), jede Seite endet mit „Zurück“ / „Schließen“; jeder Eintrag genau einmal.
//   P3 Weiter, Zentrieren, Strahl/Zielpunkt/Spiel-Mauszeiger je „aus“, Passthrough, Beenden rot mit
//      sichtbarer Bestätigung (ConfirmAction, xr_menu_model.h).
//   P2 Bedienung per Strahl oder Stick (navigate) mit sichtbarem Fokus (Hover-Rahmen).
// Wird auf der CPU gezeichnet (Canvas) und in die Swapchain eines eigenen Quad-Layers geladen.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "xr_canvas.h"
#include "xr_menu_layout.h"
#include "xr_settings.h"

namespace xrs {

enum class MenuAction {
  // Navigation
  kOpenSettings, kBack, kClose,
  kTabGame, kTabWindow, kTabControls, kTabDisplay,
  // Schnellmenü
  kResume, kRecenter, kTogglePassthrough, kGameMenu, kKeyboard, kQuit,
  // Gruppen (Schnellmenü)
  kGroupMode /* value = GroupMode */, kGroup /* value 1–9 */,
  // Spiel
  kPause, kSave, kLoad, kSpeedUp, kSpeedDown, kIdleWorker, kLastMessage, kCenterSelection, kSkipMovie,
  kToggleAutoSkip, kHelp, kToggleInputLog, kDiagnose,
  // Fenster
  kResetWindow, kSmaller, kLarger, kCloser, kFarther, kToggleView,
  kTilt, kTableLower,  // value = −1 / +1 (Tisch: Neigung in 5°, Höhe in 5 cm)
  // Steuerung
  kToggleHand, kCycleDrag, kScrollSpeed /* value ±1 */,
  kPointerOffsetX, kPointerOffsetY,  // value = −1 (‹ links/oben) / +1 (› rechts/unten)
  // Anzeige
  kToggleBeam, kCycleTarget, kToggleGameCursor, kImageReset, kCycleCrt, kToggleMenuStyle,
  kBrightness, kContrast, kSaturation, kSharpness,  // value = Änderung (−10 / +10)
};

// Aktionen, die bei gehaltenem Trigger bzw. A wiederholt werden (‹ ›-Charakter, P2).
inline bool menuActionRepeats(MenuAction a) {
  switch (a) {
    case MenuAction::kSmaller: case MenuAction::kLarger: case MenuAction::kCloser: case MenuAction::kFarther:
    case MenuAction::kBrightness: case MenuAction::kContrast: case MenuAction::kSaturation:
    case MenuAction::kSharpness: case MenuAction::kSpeedUp: case MenuAction::kSpeedDown:
    case MenuAction::kPointerOffsetX: case MenuAction::kPointerOffsetY:
    case MenuAction::kTilt: case MenuAction::kTableLower: case MenuAction::kScrollSpeed:
      return true;
    default:
      return false;
  }
}

enum class GroupMode { kSelect = 0, kStore = 1, kAdd = 2 };

// Seiten: 0 = Schnellmenü, 1–4 = Einstellungsreiter.
enum MenuPage { kPageQuick = 0, kPageGame = 1, kPageWindow = 2, kPageControls = 3, kPageDisplay = 4 };

struct MenuHit {
  MenuAction action;
  int value;
};

class VrMenu {
 public:
  static constexpr int kWidth = menu_layout::kWidth;
  static constexpr int kHeight = menu_layout::kHeight;

  VrMenu() : canvas_(kWidth, kHeight) {}
  // Aufbau aus den aktuellen Einstellungen, Statuszeile oben.
  // quitArmed: „Beenden“ ist nach dem ersten Klick scharf (ConfirmAction in xr_menu_model.h).
  void update(const Settings& settings, GroupMode groupMode, const std::string& status, bool passthroughAvailable,
              bool movieRunning, bool quitArmed);
  void setHover(int index);
  int hover() const { return hover_; }
  // u,v in 0..1 auf der Menüfläche; -1 = keine Schaltfläche.
  int hit(float u, float v) const;
  MenuHit action(int index) const;
  const std::string& label(int index) const { return elements_[static_cast<size_t>(index)].label; }
  void setPage(int page);
  int page() const { return page_; }
  // Stick-Bedienung: nächste Schaltfläche in Richtung (dx, dy ∈ {−1, 0, 1}); from < 0 = Startfokus.
  // Gesperrte Schaltflächen werden übersprungen; ohne Nachbarn bleibt der Fokus.
  int navigate(int from, int dx, int dy) const;
  // Zeichnet bei Bedarf neu; version() ändert sich mit jedem neuen Bild.
  const uint8_t* pixels();
  uint32_t version() const { return version_; }
  // Menüstil gewechselt: neu zeichnen.
  void invalidate() { dirty_ = true; }

  // Für Tests (tests/test_ui.cpp): Anzahl und Lage der Schaltflächen.
  int count() const { return static_cast<int>(elements_.size()); }
  void rect(int index, int* x, int* y, int* w, int* h) const {
    const Element& e = elements_[static_cast<size_t>(index)];
    *x = e.x; *y = e.y; *w = e.w; *h = e.h;
  }
  bool grayed(int index) const { return elements_[static_cast<size_t>(index)].grayed; }
  // Gruppenleiste (kleine Knöpfe) statt großer Schaltfläche; aktiver Modus/Reiter
  bool strip(int index) const { return elements_[static_cast<size_t>(index)].strip; }
  bool selected(int index) const { return elements_[static_cast<size_t>(index)].selected; }

 private:
  struct Element {
    bool tab;
    MenuAction action;
    int value;
    std::string label;
    int x, y, w, h;
    bool selected;  // Reiter: aktive Seite; Gruppenmodus: aktiver Modus (Schalter zeigen ihren Zustand im Text)
    bool grayed;
    bool danger = false;  // Beenden: rot
    bool armed = false;   // Beenden scharf
    bool strip = false;   // Gruppenleiste
  };
  void render();

  Canvas canvas_;
  std::vector<Element> elements_;
  std::string status_;
  int page_ = kPageQuick;
  int hover_ = -1;
  bool dirty_ = true;
  uint32_t version_ = 0;
};

}  // namespace xrs
