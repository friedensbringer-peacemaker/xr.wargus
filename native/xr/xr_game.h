// Spielpaket der App: dieselbe Engine- und XR-Schicht bedient Wargus (xr.wargus2) und War1gus
// (xr.wargus1). Alles, was sich je Spielpaket unterscheidet (Datenordner, Prüfdatei, Name der
// Einstellungstabelle, VR-Voreinstellungen, Grafiken für den Menüstil, Texte), steht hier.
// Gewählt wird nach der App-ID (Java übergibt sie als Argument, android_main.cpp setzt XRS_GAME).
// Abhängigkeitsfrei (Host-Test tests/test_game.cpp).
#pragma once

#include <cstdlib>
#include <cstring>
#include <string>

namespace xrs {

// Einmalige VR-Voreinstellung (Marker-Datei je Stufe, siehe android_main.cpp SeedPreferences).
struct PreferenceStep {
  const char* marker;
  const char* lines;
};

struct GameProfile {
  const char* id;             // Wert von XRS_GAME
  const char* appId;          // App-ID = Label (xr.<name>)
  const char* engineName;     // Spielpaket (Upstream-Name)
  const char* dataDirName;    // Unterordner von files/ mit den importierten Spieldaten
  const char* checkFile;      // muss im Datenordner lesbar sein (sonst Fehlerseite)
  const char* gameName;       // SetGameName() des Pakets: Unterordner des Benutzerordners
  const char* prefsTable;     // globale Lua-Tabelle mit .preferences
  const PreferenceStep* steps;
  int stepCount;
  // Menüstil: %s = "human"/"orc"; leer = gezeichneter Ersatz (Steingrau, Blau/Braun).
  const char* panelImage;
  const char* buttonImages[3];  // normal, gedrückt, gesperrt
  const char* fontImage;        // Glyphenblatt (Palettenbild, Zeichen ab 32, Codepage 437)
  int fontCell;                 // Kantenlänge einer Glyphenzelle
  const char* prepareScript;    // Datensatz am PC erzeugen
  const char* importCommand;    // Datensatz auf die Quest bringen
  // Spieltipp auf dem Ladebild (Zeilen mit \n getrennt) oder nullptr. Mit Tipp bleibt das Ladebild
  // mindestens kLoaderTipSeconds stehen (Trigger/A/B: weiter), sonst wäre er nach ~1,4 s schon weg.
  const char* loaderTip;
  // Karte verschieben über die mittlere Maustaste (Stratagus MouseScrollMap): Pixel Kartenweg je Mauspixel
  // (SetMouseScrollSpeedDefault des Pakets) und ungefähre Kartenmitte im Fenster (u, v), falls der Zeiger
  // beim Stick-Scrollen nicht über der Karte liegt.
  int mouseScrollSpeed;
  float mapCenterU, mapCenterV;
};

namespace game_detail {

// Wargus: 1280×800 passend zur festen Renderfläche (StratagusActivity), sonst 640×480 mit Balken;
// Kanten-Scrollen aus (der Strahl verlässt das Fenster ständig, gescrollt wird per Stick); kein
// Pausieren beim Verlassen des Fensters. Marker-Namen unverändert seit 0.2.x.
inline constexpr PreferenceStep kWargusSteps[] = {
    {"xr-defaults-v2",
     "wc2.preferences.VideoWidth = 1280\n"
     "wc2.preferences.VideoHeight = 800\n"
     "wc2.preferences.EnableMouseScrolling = false\n"
     "wc2.preferences.EnableKeyboardScrolling = true\n"},
    {"xr-defaults-v3", "wc2.preferences.PauseOnLeave = false\n"},
};

// War1gus: Oberfläche ist für 320×200 gezeichnet. 640×400 = genau die halbe Renderfläche
// (ganzzahlig 2×, scharf, gleicher Kartenanteil wie Wargus bei 1280×800). OriginalPixelRatio
// (Höhe ×1,2 wie am 4:3-Röhrenmonitor) aus, sonst Balken links/rechts. Kanten-Scrollen und
// PauseOnLeave sind in War1gus keine Einstellungen: der Datensatz schaltet das Kanten-Scrollen
// ab (scripts/prepare-data-war1.sh), PauseOnLeave ist dort fest aus.
inline constexpr PreferenceStep kWar1gusSteps[] = {
    {"xr-defaults-v1",
     "wc1.preferences.VideoWidth = 640\n"
     "wc1.preferences.VideoHeight = 400\n"
     "wc1.preferences.OriginalPixelRatio = false\n"},
};

inline constexpr GameProfile kWargus{
    "wargus",
    "xr.wargus2",
    "Wargus",
    "data.Wargus",
    "scripts/stratagus.lua",
    "wc2",
    "wc2",
    kWargusSteps,
    static_cast<int>(sizeof(kWargusSteps) / sizeof(kWargusSteps[0])),
    "graphics/ui/%s/panel_5.png",
    {"graphics/ui/%s/widgets/button-large-normal.png", "graphics/ui/%s/widgets/button-large-pressed.png",
     "graphics/ui/%s/widgets/button-large-grayed.png"},
    "graphics/ui/fonts/large.png",
    17,  // wie wargus/scripts/fonts.lua
    "scripts/prepare-data.sh",
    "scripts/quest.sh import-data",
    nullptr,
    4,  // wargus/scripts/stratagus.lua MouseScrollSpeedDefault
    0.57f, 0.5f,  // Kartenfeld rechts der 176-px-Leiste
};

// War1gus-Menüs zeichnen Schaltflächen als Farbflächen (keine Knopfgrafiken) → Ersatz-Knöpfe;
// Panel aus dem Original (ui/<volk>/panel_1.png), Schrift „large-title“ aus War1gus/contrib (14×14).
inline constexpr GameProfile kWar1gus{
    "war1gus",
    "xr.wargus1",
    "War1gus",
    "data.War1gus",
    "scripts/stratagus.lua",
    "wc1",
    "wc1",
    kWar1gusSteps,
    static_cast<int>(sizeof(kWar1gusSteps) / sizeof(kWar1gusSteps[0])),
    "graphics/ui/%s/panel_1.png",
    {"", "", ""},
    "contrib/fonts/large.png",
    14,  // wie war1gus/scripts/fonts.lua („large-title“; Codepage 437 wie die Wargus-Schrift)
    "scripts/prepare-data-war1.sh",
    "PKG=xr.wargus1 scripts/quest.sh import-data",
    // Anders als in Warcraft II braucht jedes Gebäude außer dem Rathaus eine angrenzende Straße
    // (war1gus/scripts/helpers.lua BuildingRules); im Spiel erklärt das nichts (Headset 03.10.2026).
    "Gebäude brauchen eine Straße direkt daneben (außer dem Rathaus).\n"
    "Straßen baut das Rathaus: Rathaus wählen, Build Road (R).\n"
    "Lange Straßen: A halten (Umschalt), Trigger drücken und ziehen.",
    1,  // war1gus/scripts/stratagus.lua SetMouseScrollSpeedDefault(1)
    0.6f, 0.53f,
};

}  // namespace game_detail

// Profil zu einer App-ID oder einem XRS_GAME-Wert; Unbekanntes/leer → Wargus (bisherige App).
inline const GameProfile& gameProfileFor(const char* key) {
  if (key && (!std::strcmp(key, game_detail::kWar1gus.appId) || !std::strcmp(key, game_detail::kWar1gus.id))) {
    return game_detail::kWar1gus;
  }
  return game_detail::kWargus;
}

// Profil dieses Prozesses (Umgebung XRS_GAME, gesetzt in android_main.cpp).
inline const GameProfile& currentGame() { return gameProfileFor(std::getenv("XRS_GAME")); }

// Pfad einer Menügrafik relativ zum Datenordner; race = "human"/"orc". Leer, wenn das Paket keine hat.
inline std::string themeAsset(const char* pattern, bool orc) {
  if (!pattern || !*pattern) return std::string();
  std::string out;
  for (const char* p = pattern; *p; ++p) {
    if (p[0] == '%' && p[1] == 's') {
      out += orc ? "orc" : "human";
      ++p;
    } else {
      out += *p;
    }
  }
  return out;
}

// App-ID aus den Startargumenten (Java: "--xr-app=<id>"); nullptr, wenn keins dabei ist.
inline const char* appIdFromArg(const char* arg) {
  static constexpr char kPrefix[] = "--xr-app=";
  if (!arg || std::strncmp(arg, kPrefix, sizeof(kPrefix) - 1) != 0) return nullptr;
  return arg + sizeof(kPrefix) - 1;
}

}  // namespace xrs
