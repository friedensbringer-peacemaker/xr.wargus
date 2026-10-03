// Layout-Raster des VR-Menüs (Texturpixel; Skill xr-vr-menu P5: ein Raster, keine Einzelwerte).
// Schaltflächen sind die Wargus-Knöpfe 224×28 in 1,75-facher Größe.
#pragma once

namespace xrs::menu_layout {

constexpr int kWidth = 896;
constexpr int kHeight = 800;
constexpr int kMargin = 32;
constexpr int kGap = 12;
constexpr int kTitleH = 40;
constexpr int kStatusY = 60;
constexpr int kTabY = 100;     // Reiterzeile (nur Einstellungen)
constexpr int kTabH = 46;
constexpr int kQuickY = 100;   // erste Zeile des Schnellmenüs
constexpr int kQuickRowH = 50; // große Knöpfe im Schnellmenü
constexpr int kQuickGap = 10;
// Gruppenleiste im Schnellmenü: Modus (Wählen/Speichern/Ergänzen) und schmale Knöpfe 1–9
constexpr int kGroupModeY = kQuickY + 8 * (kQuickRowH + kQuickGap) + 4;
constexpr int kGroupModeH = 44;
constexpr int kGroupY = kGroupModeY + kGroupModeH + 10;
constexpr int kGroupH = 52;
constexpr int kGroupGap = 8;
constexpr int kGroupModes = 3;
constexpr int kGroups = 9;
constexpr int kContentY = 164; // erste Zeile der Einstellungsseiten
constexpr int kRowH = 49;
constexpr int kColumns = 2;
constexpr int kFooterY = kHeight - 42;
constexpr int kContentW = kWidth - 2 * kMargin;
constexpr int kCellW = (kContentW - (kColumns - 1) * kGap * 2) / kColumns;

constexpr int row(int i) { return kContentY + i * (kRowH + kGap); }
constexpr int quickRow(int i) { return kQuickY + i * (kQuickRowH + kQuickGap); }
constexpr int groupModeW() { return (kContentW - (kGroupModes - 1) * kGroupGap) / kGroupModes; }
constexpr int groupModeX(int i) { return kMargin + i * (groupModeW() + kGroupGap); }
constexpr int groupW() { return (kContentW - (kGroups - 1) * kGroupGap) / kGroups; }
constexpr int groupX(int i) { return kMargin + i * (groupW() + kGroupGap); }
constexpr int column(int c) { return kMargin + c * (kCellW + 2 * kGap); }

}  // namespace xrs::menu_layout
