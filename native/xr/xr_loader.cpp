#include "xr_loader.h"

#include "xr_game.h"
#include "xr_theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace xrs {
namespace {


constexpr Rgb kTitle{252, 212, 60};  // Wargus-Gold
constexpr Rgb kText{235, 238, 242};
constexpr Rgb kDim{140, 148, 160};
constexpr Rgb kBarBack{20, 20, 20};
constexpr Rgb kBar{30, 52, 116};
constexpr Rgb kBarGlow{110, 140, 220};
constexpr Rgb kDone{90, 190, 120};
constexpr Rgb kActive{240, 190, 70};
constexpr Rgb kPending{70, 78, 92};
constexpr Rgb kError{235, 90, 80};
constexpr Rgb kTipBack{28, 30, 38};

constexpr int kMargin = 56;

std::string stepText(int i) {
  switch (i) {
    case 0: return "VR-Darstellung gestartet";
    case 1: return std::string("Eigene Spieldaten gefunden (") + currentGame().dataDirName + ")";
    case 2: return "Stratagus lädt Grafiken, Sounds und Skripte";
    default: return "Intro bzw. Hauptmenü wird vorbereitet";
  }
}

// Fortschritt: je Schritt ein Viertel, innerhalb des aktiven Schritts zeitbasiert
// (nähert sich asymptotisch dem Schrittende – ehrlich „läuft noch“, nie „fertig“).
float targetProgress(const LoaderState& s) {
  int step = static_cast<int>(s.step);
  if (step >= 4) return 1.0f;
  float within = 1.0f - std::exp(-s.stepSeconds / 4.0f);
  return (static_cast<float>(step) + 0.9f * within) / 4.0f;
}

}  // namespace

const uint8_t* LoaderPanel::pixels(const LoaderState& state) {
  // Balken weich nachführen, nie zurücklaufen.
  float target = targetProgress(state);
  shownProgress_ = std::max(shownProgress_, shownProgress_ + (target - shownProgress_) * 0.2f);
  int tick = static_cast<int>(state.seconds * 15.0f);
  char key[64];
  std::snprintf(key, sizeof(key), "%d|%d|%d|%zu", static_cast<int>(state.step), state.error,
                state.dataMissing, state.errorText.size());
  if (tick != lastTick_ || lastKey_ != key) {
    lastTick_ = tick;
    lastKey_ = key;
    render(state, shownProgress_);
    ++version_;
  }
  return canvas_.data();
}

void LoaderPanel::render(const LoaderState& s, float progress) {
  Theme::get().drawPanel(canvas_, 0, 0, kWidth, kHeight);
  const int contentW = kWidth - 2 * kMargin;

  if (s.error || s.dataMissing) {
    canvas_.drawText(s.dataMissing ? "Originalspieldaten fehlen" : "Start fehlgeschlagen", kMargin, 96, 46, kError);
    int y = 160;
    const GameProfile& game = currentGame();
    std::string message = s.dataMissing ? std::string("Im App-Speicher liegen keine Spieldaten (") + game.dataDirName +
                                              "/" + game.checkFile + "). Am PC: " + game.prepareScript + " und " +
                                              game.importCommand
                                        : "Die Spiel-Engine hat einen Fehler gemeldet:";
    for (const std::string& line : canvas_.wrap(message, contentW, 28)) {
      canvas_.drawText(line, kMargin, y, 28, kText);
      y += 40;
    }
    if (!s.errorText.empty()) {
      y += 10;
      for (const std::string& line : canvas_.wrap(s.errorText, contentW, 24)) {
        canvas_.drawText(line, kMargin, y, 24, kActive);
        y += 34;
        if (y > kHeight - 200) break;
      }
    }
    int base = kHeight - 140;
    canvas_.drawText("Das Log ist gespeichert. Am PC abholen: scripts/quest.sh logs", kMargin, base, 24, kDim);
    for (const std::string& line : canvas_.wrap(s.logPath, contentW, 20)) {
      base += 30;
      canvas_.drawText(line, kMargin, base, 20, kDim);
    }
    canvas_.drawText("B: Spielbild trotzdem anzeigen", kMargin, kHeight - 40, 26, kText);
    return;
  }

  canvas_.drawShadowText(currentGame().appId, kMargin, 96, 52, kTitle, {0, 0, 0});
  canvas_.drawText("Das Spiel wird geladen …", kMargin, 150, 32, kText);

  // Fortschrittsbalken mit wanderndem Lichtpunkt (zeigt: es läuft noch).
  const int barY = 190, barH = 34;
  canvas_.fillRect(kMargin, barY, contentW, barH, kBarBack);
  int filled = static_cast<int>(static_cast<float>(contentW) * std::clamp(progress, 0.0f, 1.0f));
  canvas_.fillRect(kMargin, barY, filled, barH, kBar);
  if (filled > 40) {
    float phase = std::fmod(s.seconds * 0.6f, 1.0f);
    int glowX = kMargin + static_cast<int>(phase * static_cast<float>(filled - 40));
    canvas_.fillRect(glowX, barY, 40, barH, kBarGlow);
  }
  char pct[16];
  std::snprintf(pct, sizeof(pct), "%d %%", static_cast<int>(progress * 100.0f + 0.5f));
  canvas_.drawText(pct, kMargin + contentW - canvas_.textWidth(pct, 26), barY + barH + 34, 26, kDim);

  // Schritte
  int y = 300;
  int current = static_cast<int>(s.step);
  for (int i = 0; i < 4; ++i) {
    Rgb dot = i < current ? kDone : i == current ? kActive : kPending;
    if (i == current && static_cast<int>(s.seconds * 2.0f) % 2 == 0) dot = kBarGlow;
    canvas_.fillRect(kMargin, y - 22, 22, 22, dot);
    canvas_.drawText(stepText(i), kMargin + 40, y, 28, i <= current ? kText : kDim);
    y += 52;
  }

  // Spieltipp (aus dem Spielpaket): heller Kasten unter den Schritten
  if (const char* tip = currentGame().loaderTip) {
    const int boxY = 530, boxH = 150;
    canvas_.fillRect(kMargin - 12, boxY, contentW + 24, boxH, kTipBack);
    canvas_.fillRect(kMargin - 12, boxY, 6, boxH, kActive);
    canvas_.drawText("Tipp", kMargin + 4, boxY + 34, 28, kActive);
    int ty = boxY + 70;
    std::string text = tip;
    for (size_t start = 0; start <= text.size();) {
      size_t end = text.find('\n', start);
      if (end == std::string::npos) end = text.size();
      canvas_.drawText(text.substr(start, end - start), kMargin + 4, ty, 23, kText);
      ty += 30;
      start = end + 1;
    }
    if (s.step == LoadStep::kGame) {
      canvas_.drawText("Trigger, A oder B: weiter", kMargin, kHeight - 24, 22, kActive);
    }
    char elapsed[48];
    std::snprintf(elapsed, sizeof(elapsed), "seit %d s", static_cast<int>(s.seconds));
    canvas_.drawText(elapsed, kMargin + contentW - canvas_.textWidth(elapsed, 22), kHeight - 24, 22, kDim);
    return;
  }

  // Bitte um Geduld + Laufzeit
  int base = kHeight - 110;
  canvas_.drawText("Bitte etwas Geduld – der erste Start nach einer Installation", kMargin, base, 26, kText);
  canvas_.drawText("dauert länger, weil Spieldateien vorbereitet werden.", kMargin, base + 36, 26, kText);
  char elapsed[48];
  std::snprintf(elapsed, sizeof(elapsed), "seit %d s", static_cast<int>(s.seconds));
  canvas_.drawText(elapsed, kMargin + contentW - canvas_.textWidth(elapsed, 22), kHeight - 24, 22, kDim);
}

}  // namespace xrs
