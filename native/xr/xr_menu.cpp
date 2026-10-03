#include "xr_menu.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "xr_theme.h"

namespace xrs {
namespace {

using namespace menu_layout;

constexpr int kTabs = 3;
constexpr Rgb kHoverFrame{252, 212, 60};  // Wargus-Gold

const char* onOff(bool on) { return on ? "an" : "aus"; }

}  // namespace

void VrMenu::setPage(int page) {
  if (page != page_) {
    page_ = page;
    hover_ = -1;
    elements_.clear();  // beim nächsten update neu aufbauen
    dirty_ = true;
  }
}

void VrMenu::update(const Settings& s, GroupMode groupMode, const std::string& status, bool passthroughAvailable,
                    bool movieRunning, bool quitArmed) {
  std::vector<Element> e;
  // Schalter zeigen ihren Zustand nur im Text („Strahl: an“), nie als gedrückte Schaltfläche.
  auto add = [&](MenuAction action, const std::string& label, int x, int y, int w, int h, int value = 0,
                 bool grayed = false) {
    e.push_back({false, action, value, label, x, y, w, h, false, grayed});
    return &e.back();
  };
  const std::string passthrough =
      passthroughAvailable ? std::string("Passthrough: ") + onOff(s.passthrough) : "Passthrough: fehlt";

  if (page_ == kPageQuick) {
    // Stufe 1: wenige große Knöpfe (P5), Beenden rot mit sichtbarer Bestätigung (P3).
    int r = 0;
    add(MenuAction::kResume, "Weiterspielen", kMargin, quickRow(r++), kContentW, kQuickRowH);
    add(MenuAction::kRecenter, "Fenster zentrieren", kMargin, quickRow(r++), kContentW, kQuickRowH);
    add(MenuAction::kTogglePassthrough, passthrough, kMargin, quickRow(r++), kContentW, kQuickRowH, 0,
        !passthroughAvailable);
    add(MenuAction::kGameMenu, "Spielmenü (F10)", kMargin, quickRow(r++), kContentW, kQuickRowH);
    add(MenuAction::kKeyboard, "Tastatur", kMargin, quickRow(r++), kContentW, kQuickRowH);
    add(MenuAction::kOpenSettings, "Einstellungen", kMargin, quickRow(r++), kContentW, kQuickRowH);
    Element* quit = add(MenuAction::kQuit, quitArmed ? "Sicher? Nochmal klicken" : "Beenden", kMargin,
                        quickRow(r++), kContentW, kQuickRowH);
    quit->danger = true;
    quit->armed = quitArmed;
    add(MenuAction::kClose, "Schließen", kMargin, quickRow(r++), kContentW, kQuickRowH);
    // Gruppenleiste: Modus als Auswahl (aktiver hervorgehoben), darunter schmale Knöpfe 1–9.
    static const char* modes[kGroupModes] = {"Wählen", "Speichern", "Ergänzen"};
    for (int m = 0; m < kGroupModes; ++m) {
      Element* b = add(MenuAction::kGroupMode, modes[m], groupModeX(m), kGroupModeY, groupModeW(), kGroupModeH, m);
      b->selected = static_cast<int>(groupMode) == m;
      b->strip = true;
    }
    for (int n = 1; n <= kGroups; ++n) {
      Element* b = add(MenuAction::kGroup, std::to_string(n), groupX(n - 1), kGroupY, groupW(), kGroupH, n);
      b->strip = true;
    }
  } else {
    // Stufe 2: Einstellungen mit Reitern.
    const int tabW = (kContentW - (kTabs - 1) * kGap) / kTabs;
    const char* tabs[kTabs] = {"Spiel", "Fenster", "Anzeige"};
    const MenuAction tabActions[kTabs] = {MenuAction::kTabGame, MenuAction::kTabWindow, MenuAction::kTabDisplay};
    for (int t = 0; t < kTabs; ++t) {
      e.push_back({true, tabActions[t], 0, tabs[t], kMargin + t * (tabW + kGap), kTabY, tabW, kTabH,
                   page_ == t + 1, false});
    }
    int i = 0;
    auto grid = [&](MenuAction action, const std::string& label, int value = 0, bool grayed = false) {
      add(action, label, column(i % kColumns), row(i / kColumns), kCellW, kRowH, value, grayed);
      ++i;
    };
    if (page_ == kPageGame) {
      grid(MenuAction::kPause, "Pause an/aus");
      grid(MenuAction::kHelp, "Hilfe (F1)");
      grid(MenuAction::kSave, "Speichern (F11)");
      grid(MenuAction::kLoad, "Laden (F12)");
      grid(MenuAction::kSpeedUp, "Tempo +");
      grid(MenuAction::kSpeedDown, "Tempo -");
      grid(MenuAction::kIdleWorker, "Untätiger Arbeiter");
      grid(MenuAction::kLastMessage, "Letzte Meldung");
      grid(MenuAction::kCenterSelection, "Auswahl zentrieren");
      grid(MenuAction::kSkipMovie, "Film überspringen", 0, !movieRunning);
      grid(MenuAction::kToggleAutoSkip, std::string("Filme auslassen: ") + onOff(s.autoSkipMovies));
      grid(MenuAction::kToggleInputLog, std::string("Eingaben loggen: ") + onOff(s.logInput));
      grid(MenuAction::kDiagnose, "Diagnose ins Log");
    } else if (page_ == kPageWindow) {
      grid(MenuAction::kResetWindow, "Fenster zurücksetzen");
      grid(MenuAction::kToggleHand, std::string("Zeigerhand: ") + (s.pointerHand ? "rechts" : "links"));
      grid(MenuAction::kSmaller, "Fenster kleiner");
      grid(MenuAction::kLarger, "Fenster größer");
      grid(MenuAction::kCloser, "Fenster näher");
      grid(MenuAction::kFarther, "Fenster weiter weg");
      grid(MenuAction::kToggleMenuStyle, std::string("Menüstil: ") + (s.menuOrc ? "Ork" : "Mensch"));
    } else {
      const char* target = s.target == TargetStyle::kOff ? "aus" : s.target == TargetStyle::kRing ? "Ring" : "Punkt";
      char value[48];
      grid(MenuAction::kToggleBeam, std::string("Strahl: ") + onOff(s.beam));
      grid(MenuAction::kCycleTarget, std::string("Zielpunkt: ") + target);
      grid(MenuAction::kToggleGameCursor, std::string("Spiel-Mauszeiger: ") + onOff(s.gameCursor));
      grid(MenuAction::kImageReset, "Bild zurücksetzen");
      static const char* crtModes[] = {"normal", "Scanlines", "CRT"};
      grid(MenuAction::kCycleCrt, std::string("Bildmodus: ") + crtModes[std::clamp(s.image.crt, 0, 2)]);
      std::snprintf(value, sizeof(value), "Helligkeit + (%d)", s.image.brightness);
      grid(MenuAction::kBrightness, "Helligkeit -", -10);
      grid(MenuAction::kBrightness, value, 10);
      std::snprintf(value, sizeof(value), "Kontrast + (%d)", s.image.contrast);
      grid(MenuAction::kContrast, "Kontrast -", -10);
      grid(MenuAction::kContrast, value, 10);
      std::snprintf(value, sizeof(value), "Sättigung + (%d)", s.image.saturation);
      grid(MenuAction::kSaturation, "Sättigung -", -10);
      grid(MenuAction::kSaturation, value, 10);
      std::snprintf(value, sizeof(value), "Schärfe + (%d)", s.image.sharpness);
      grid(MenuAction::kSharpness, "Schärfe -", -10);
      grid(MenuAction::kSharpness, value, 10);
    }
    // Jede Seite endet mit „Zurück“ (Schnellmenü) und „Schließen“.
    const int last = row((i + kColumns - 1) / kColumns);
    add(MenuAction::kBack, "Zurück", column(0), last, kCellW, kRowH);
    add(MenuAction::kClose, "Schließen", column(1), last, kCellW, kRowH);
  }

  bool changed = status != status_ || e.size() != elements_.size();
  for (size_t i = 0; !changed && i < e.size(); ++i) {
    changed = e[i].label != elements_[i].label || e[i].selected != elements_[i].selected ||
              e[i].grayed != elements_[i].grayed || e[i].armed != elements_[i].armed;
  }
  if (changed) {
    elements_ = std::move(e);
    status_ = status;
    if (hover_ >= static_cast<int>(elements_.size())) hover_ = -1;
    dirty_ = true;
  }
}

void VrMenu::setHover(int index) {
  if (index != hover_) {
    hover_ = index;
    dirty_ = true;
  }
}

int VrMenu::hit(float u, float v) const {
  int x = static_cast<int>(u * kWidth);
  int y = static_cast<int>(v * kHeight);
  for (size_t i = 0; i < elements_.size(); ++i) {
    const Element& b = elements_[i];
    if (b.grayed) continue;
    if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) return static_cast<int>(i);
  }
  return -1;
}

int VrMenu::navigate(int from, int dx, int dy) const {
  const int n = static_cast<int>(elements_.size());
  if (n == 0) return -1;
  if (from < 0 || from >= n) {
    // Startfokus: erste klickbare Nicht-Reiter-Schaltfläche (Schnellmenü: „Weiterspielen“).
    for (int i = 0; i < n; ++i) {
      if (!elements_[static_cast<size_t>(i)].tab && !elements_[static_cast<size_t>(i)].grayed) return i;
    }
    return 0;
  }
  if (dx == 0 && dy == 0) return from;
  const Element& a = elements_[static_cast<size_t>(from)];
  const int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
  // Erst nur Schaltflächen, die quer zur Richtung überlappen (gleiche Zeile bzw. Spalte), dann alle.
  int best = from;
  for (int pass = 0; pass < 2 && best == from; ++pass) {
    long bestScore = 0;
    for (int i = 0; i < n; ++i) {
      const Element& b = elements_[static_cast<size_t>(i)];
      if (i == from || b.grayed) continue;
      const int bx = b.x + b.w / 2, by = b.y + b.h / 2;
      const int along = dx != 0 ? (bx - ax) * dx : (by - ay) * dy;  // Abstand in Bewegungsrichtung
      const int across = dx != 0 ? std::abs(by - ay) : std::abs(bx - ax);
      if (along <= 0) continue;
      const bool overlaps = dx != 0 ? (b.y < a.y + a.h && a.y < b.y + b.h) : (b.x < a.x + a.w && a.x < b.x + b.w);
      if (pass == 0 && !overlaps) continue;
      const long score = static_cast<long>(along) + 2L * across;
      if (best == from || score < bestScore) {
        best = i;
        bestScore = score;
      }
    }
  }
  return best;
}

MenuHit VrMenu::action(int index) const {
  const Element& b = elements_[static_cast<size_t>(index)];
  return {b.action, b.value};
}

const uint8_t* VrMenu::pixels() {
  if (dirty_) {
    render();
    dirty_ = false;
    ++version_;
  }
  return canvas_.data();
}

void VrMenu::render() {
  const Theme& theme = Theme::get();
  theme.drawPanel(canvas_, 0, 0, kWidth, kHeight);
  theme.drawCentered(canvas_, page_ == kPageQuick ? "VR-Menü" : "Einstellungen", 0, 16, kWidth, kTitleH, 2,
                     TextColor::kNormal);
  theme.drawCentered(canvas_, status_, kMargin, kStatusY, kContentW, 30, 1, TextColor::kHighlight);
  for (size_t i = 0; i < elements_.size(); ++i) {
    const Element& b = elements_[i];
    ButtonState st = b.grayed                          ? ButtonState::kGrayed
                     : b.selected                      ? ButtonState::kPressed
                     : static_cast<int>(i) == hover_   ? ButtonState::kHover
                                                       : ButtonState::kNormal;
    // Fokus (Strahl oder Stick): goldener Rahmen um die Schaltfläche (passt in den Abstand).
    bool hot = static_cast<int>(i) == hover_ && !b.grayed;
    if (hot) canvas_.fillRect(b.x - 4, b.y - 4, b.w + 8, b.h + 8, kHoverFrame);
    // Aktiver Gruppenmodus: zusätzlich goldene Unterstreichung (das Bild „gedrückt“ allein ist kaum zu sehen).
    if (b.strip && b.selected) canvas_.fillRect(b.x + 8, b.y + b.h + 3, b.w - 16, 4, kHoverFrame);
    if (b.danger) {
      theme.drawDangerButton(canvas_, b.x, b.y, b.w, b.h, b.armed, hot, b.label);
    } else {
      theme.drawButton(canvas_, b.x, b.y, b.w, b.h, st, b.label);
    }
  }
  theme.drawCentered(canvas_, "Strahl + Trigger oder Stick + A  -  B / Menütaste: schließen", 0, kFooterY, kWidth,
                     30, 1, TextColor::kNormal);
}

}  // namespace xrs
