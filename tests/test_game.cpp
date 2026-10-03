// Spielpaket-Profile (native/xr/xr_game.h): Auswahl nach App-ID/XRS_GAME, Pfade, Namensregel,
// VR-Voreinstellungen – und dass xr.wargus2 (Wargus) exakt die bisherigen Werte behält.
#include <cctype>
#include <cstdio>
#include <set>
#include <string>

#include "xr_game.h"

using namespace xrs;

namespace {

int g_checks = 0, g_failures = 0;

void check(bool ok, const std::string& what) {
  ++g_checks;
  if (!ok) {
    ++g_failures;
    if (g_failures <= 20) std::printf("FEHLER: %s\n", what.c_str());
  }
}

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

const GameProfile* const kAll[] = {&game_detail::kWargus, &game_detail::kWar1gus};

void testSelection() {
  // Jede bekannte App-ID und jeder XRS_GAME-Wert führt auf sein Profil.
  for (const GameProfile* g : kAll) {
    check(&gameProfileFor(g->appId) == g, std::string("App-ID ") + g->appId);
    check(&gameProfileFor(g->id) == g, std::string("XRS_GAME ") + g->id);
  }
  // Unbekanntes, leer, nullptr, Varianten → bisherige App (Wargus), nie War1gus aus Versehen.
  const char* others[] = {nullptr, "", "xr.wargus2.debug", "xr.wargus1.", "XR.WARGUS1", "war1", "wargus1",
                          "xr.war2gus", " xr.wargus1", "xr.wargus", "xr.war1gus" /* alte App-ID */};
  for (const char* o : others) {
    check(&gameProfileFor(o) == &game_detail::kWargus, std::string("Rückfall Wargus für '") + (o ? o : "null") + "'");
  }
}

void testArgs() {
  check(appIdFromArg(nullptr) == nullptr, "kein Argument");
  check(appIdFromArg("") == nullptr, "leeres Argument");
  check(appIdFromArg("-d") == nullptr, "Engine-Argument bleibt");
  check(appIdFromArg("--xr-app") == nullptr, "ohne = kein Treffer");
  const char* id = appIdFromArg("--xr-app=xr.wargus1");
  check(id && std::string(id) == "xr.wargus1", "--xr-app=xr.wargus1");
  id = appIdFromArg("--xr-app=");
  check(id && *id == '\0', "--xr-app= leer → Rückfall über gameProfileFor");
  check(&gameProfileFor(appIdFromArg("--xr-app=xr.wargus1")) == &game_detail::kWar1gus, "Argument → War1gus");
  check(&gameProfileFor(appIdFromArg("--xr-app=xr.wargus2")) == &game_detail::kWargus, "Argument → Wargus");
}

void testThemeAsset() {
  // Sweep über alle Muster beider Profile und beide Völker.
  for (const GameProfile* g : kAll) {
    const char* patterns[] = {g->panelImage, g->buttonImages[0], g->buttonImages[1], g->buttonImages[2], g->fontImage};
    for (const char* p : patterns) {
      for (int orc = 0; orc < 2; ++orc) {
        std::string out = themeAsset(p, orc != 0);
        std::string name = std::string(g->appId) + " " + p + (orc ? " Ork" : " Mensch");
        if (!*p) {
          check(out.empty(), name + ": leer bleibt leer (Ersatz)");
          continue;
        }
        check(out.find('%') == std::string::npos, name + ": Platzhalter ersetzt");
        check(out.find(orc ? "human" : "orc/") == std::string::npos, name + ": nur das gewählte Volk");
        check(out.size() > 4 && out.compare(out.size() - 4, 4, ".png") == 0, name + ": PNG");
        check(out[0] != '/', name + ": relativ zum Datenordner");
      }
    }
  }
  check(themeAsset(nullptr, false).empty(), "nullptr → leer");
  check(themeAsset("a%sb%s", true) == "aorcborc", "mehrfacher Platzhalter");
  check(themeAsset("100%", false) == "100%", "einzelnes % bleibt");
}

void testProfiles() {
  std::set<std::string> appIds, dataDirs, ids;
  for (const GameProfile* g : kAll) {
    const std::string app = g->appId;
    // Namensregel: Label = App-ID = xr.<name>, klein, kein Markenname.
    check(app.rfind("xr.", 0) == 0 && app.size() > 3, app + ": beginnt mit xr.");
    check(app == lower(app), app + ": nur Kleinbuchstaben");
    for (const char* brand : {"warcraft", "blizzard", "orcs", "humans"}) {
      check(lower(app).find(brand) == std::string::npos, app + ": kein Markenname " + brand);
    }
    check(std::string(g->dataDirName).rfind("data.", 0) == 0, app + ": Datenordner data.*");
    check(g->fontCell >= 8 && g->fontCell <= 32, app + ": Glyphenzelle plausibel");
    check(*g->panelImage && *g->fontImage, app + ": Panel und Schrift angegeben");
    check(g->stepCount >= 1, app + ": VR-Voreinstellungen vorhanden");
    // Jede Voreinstellung schreibt nur in die Tabelle des eigenen Pakets, Marker eindeutig.
    std::set<std::string> markers;
    const std::string prefix = std::string(g->prefsTable) + ".preferences.";
    for (int i = 0; i < g->stepCount; ++i) {
      const PreferenceStep& s = g->steps[i];
      check(markers.insert(s.marker).second, app + ": Marker eindeutig " + s.marker);
      std::string lines = s.lines;
      check(!lines.empty() && lines.back() == '\n', app + ": Zeilen enden mit Zeilenumbruch");
      size_t pos = 0;
      while (pos < lines.size()) {
        size_t end = lines.find('\n', pos);
        std::string line = lines.substr(pos, end - pos);
        check(line.rfind(prefix, 0) == 0, app + ": " + line + " gehört zu " + prefix);
        pos = end + 1;
      }
    }
    appIds.insert(app);
    dataDirs.insert(g->dataDirName);
    ids.insert(g->id);
  }
  check(appIds.size() == 2 && dataDirs.size() == 2 && ids.size() == 2, "Profile unterscheiden sich");
}

// xr.wargus2 darf sich nicht verändern: Werte wie vor der Parametrisierung (0.2.5-xr).
void testWargusUnchanged() {
  const GameProfile& w = game_detail::kWargus;
  check(std::string(w.appId) == "xr.wargus2", "Wargus App-ID");
  check(std::string(w.dataDirName) == "data.Wargus", "Wargus Datenordner");
  check(std::string(w.checkFile) == "scripts/stratagus.lua", "Wargus Prüfdatei");
  check(std::string(w.gameName) == "wc2" && std::string(w.prefsTable) == "wc2", "Wargus wc2");
  check(themeAsset(w.panelImage, false) == "graphics/ui/human/panel_5.png", "Wargus Panel Mensch");
  check(themeAsset(w.panelImage, true) == "graphics/ui/orc/panel_5.png", "Wargus Panel Ork");
  check(themeAsset(w.buttonImages[0], false) == "graphics/ui/human/widgets/button-large-normal.png", "Knopf normal");
  check(themeAsset(w.buttonImages[1], true) == "graphics/ui/orc/widgets/button-large-pressed.png", "Knopf gedrückt");
  check(themeAsset(w.buttonImages[2], false) == "graphics/ui/human/widgets/button-large-grayed.png", "Knopf gesperrt");
  check(std::string(w.fontImage) == "graphics/ui/fonts/large.png" && w.fontCell == 17, "Wargus Schrift");
  check(w.stepCount == 2, "Wargus zwei Stufen");
  check(std::string(w.steps[0].marker) == "xr-defaults-v2" &&
            std::string(w.steps[0].lines) ==
                "wc2.preferences.VideoWidth = 1280\nwc2.preferences.VideoHeight = 800\n"
                "wc2.preferences.EnableMouseScrolling = false\nwc2.preferences.EnableKeyboardScrolling = true\n",
        "Wargus Stufe v2 unverändert");
  check(std::string(w.steps[1].marker) == "xr-defaults-v3" &&
            std::string(w.steps[1].lines) == "wc2.preferences.PauseOnLeave = false\n",
        "Wargus Stufe v3 unverändert");
}

void testWar1gus() {
  const GameProfile& w = game_detail::kWar1gus;
  check(std::string(w.appId) == "xr.wargus1" && std::string(w.dataDirName) == "data.War1gus", "War1gus IDs");
  check(std::string(w.gameName) == "wc1" && std::string(w.prefsTable) == "wc1", "War1gus wc1 (SetGameName)");
  check(themeAsset(w.panelImage, true) == "graphics/ui/orc/panel_1.png", "War1gus Panel Ork");
  check(themeAsset(w.buttonImages[0], false).empty(), "War1gus ohne Knopfgrafik → Ersatz");
  check(std::string(w.fontImage) == "contrib/fonts/large.png" && w.fontCell == 14, "War1gus Schrift");
  // Renderfläche 1280×800: Spielauflösung teilt sie ganzzahlig (scharf, ohne Balken).
  std::string lines = w.steps[0].lines;
  check(lines.find("VideoWidth = 640\n") != std::string::npos && lines.find("VideoHeight = 400\n") != std::string::npos,
        "War1gus 640×400");
  check(1280 % 640 == 0 && 800 % 400 == 0 && 1280 / 640 == 800 / 400, "640×400 = 1280×800 / 2");
  check(lines.find("OriginalPixelRatio = false") != std::string::npos, "War1gus ohne 1,2-fache Höhe (keine Balken)");
}

}  // namespace

int main() {
  testSelection();
  testArgs();
  testThemeAsset();
  testProfiles();
  testWargusUnchanged();
  testWar1gus();
  std::printf("test_game: %d Prüfungen, %d Fehler\n", g_checks, g_failures);
  return g_failures ? 1 : 0;
}
