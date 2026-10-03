# Roadmap: Stratagus/Wargus auf der Meta Quest 3

Stand 2026-09-27. Muster wie `../XR-Settlers2.5`: eigenständige Android-App (SDLActivity)
mit engine-internem OpenXR-Ausgabepfad. Nicht über den libretro-Weg der XRShell.

## Technische Befunde (Upstream `master`)

- **Genau eine Bildausgabe:** `src/video/sdl.cpp:844–854`: `TheScreen` (SDL_Surface,
  Software) → `SDL_UpdateTexture(TheTexture)` → `RenderWithShader()` oder
  `SDL_RenderCopy` → `SDL_RenderPresent`. Hier setzt der OpenXR-Pfad an: Pixel von
  `TheScreen` in eine OpenXR-Quad-Swapchain kopieren statt ins Fenster.
- **Zweite Ausgabe:** Zwischensequenzen in `src/video/movie.cpp:235` (eigenes
  `SDL_RenderPresent` mit YUV-Textur). Muss ebenfalls umgeleitet oder anfangs übersprungen werden.
- **Shader** (`src/video/shaders.cpp`) sind Desktop-OpenGL/GLSL (CRT, xBRZ …): auf Android
  aus. Bildaufbereitung übernimmt der eigene Kopierpfad.
- **Cross-Compile ist vorgesehen:** CMake-Option `STRATAGUS_HOST_TOLUAPP` (tolua++-Generator
  muss für den Host gebaut werden, erzeugt C++ aus `src/tolua/*.pkg`), dazu
  `BUILD_VENDORED_LUA/SDL/MEDIA_LIBS` aus `third-party/`.
- **Musik** der Battle.net-Edition ist MIDI (+ `.sf2`). Unter Windows spielt ein externer
  `stratagus-midiplayer.exe`; auf Android muss SDL2_mixer mit TIMIDITY (Config unter
  `<LibPath>/timidity/timidity.cfg`, siehe `src/sound/sound_server.cpp:696`) oder
  FluidSynth (`USE_FLUIDSYNTH`) laufen.
- **Upstream kennt kein Android** (kein `__ANDROID__` im Code). Vorlage: Android-Fork
  https://github.com/drodin/Stratagus (älterer Stand), Option `ENABLE_TOUCHSCREEN`.
- **Daten:** `wartool` akzeptiert DOS-CD, Battle.net-Edition (`INSTALL.MPQ`) und GOG.
  Lokal erfolgreich extrahiert (`original-data/wargus-desktop`, ~800 MB inkl. Videos),
  Desktop-Start mit `stratagus.exe -d .` ohne Absturz.

## Schritte

### S0 — Grundlage ✅
Quellen als Submodule, Windows-Release 3.3.2, GOG-Daten extrahiert, Desktop läuft.

### S1 — Flacher Android-Build (ohne VR) — 🟡 fast fertig
Stand 2026-09-27, **auf der Quest 3 getestet:** APK startet, Renderer `opengles2`, Hauptmenü
und Optionsmenü erscheinen als 2D-Panel, Controller-Laser bedient das Menü.

Nachgebessert (gebaut, Headset-Abnahme durch den Spieler steht aus):
- Musik: Endung `.mid` statt `.wav` im Datensatz, TiMidity + Freepats → Log meldet jetzt
  `TIMIDITY MIDI` als Musik-Decoder
- Video-Ton: SDL_mixer mit libvorbisfile statt stb_vorbis
- 1280×800 statt 640×480 beim ersten Start (keine Balken im Panel)
- Schließen des Panels beendet die App sofort (vorher 10-s-Timeout) — auf dem Gerät bestätigt
- Gemeinsames SDK `_tools/android-sdk`, App-Label `xr.wargus2`

Offen: Einzelspieler-Karte/Kampagne im Headset spielen, Musik/Video-Ton hören, Lesbarkeit bei
1280×800 beurteilen.

Ziel: Wargus startet als normale 2D-Android-App auf der Quest (Panel-Fenster) bis ins Menü.
1. Gradle-Projekt `android/` mit SDL2-`SDLActivity` (SDL aus `third-party/SDL`), arm64-v8a,
   minSdk 29, Paket-ID ohne Markennamen (Vorschlag `xr.wargus2`).
2. Host-tolua++ bauen (Windows, vendored Lua) → `STRATAGUS_HOST_TOLUAPP`.
3. Stratagus + SDL2_image/mixer + Lua 5.1 + png/zlib per NDK bauen; Shader, MNG, Theora,
   OpenMP zunächst aus. Desktop-Pfade (`StratagusLibPath`, `wargus.exe`-Launcher) auf
   App-Speicher umbiegen.
4. Datenimport: extrahierten Ordner per `adb push` nach `/sdcard/Stratagus/data.Wargus`
   (Muster Settlers `quest.py import-data`). Kein `wartool` auf dem Gerät.
5. Abnahme: Menü sichtbar, Einzelspieler-Karte startet, Ton an.

### S2 + S3 — VR-Darstellung, Steuerung, VR-Menü — 🟡 gebaut (0.2.0-xr), Headset-Abnahme offen
Umgesetzt 2026-09-29 mit der XR-Schicht aus XR-CorsixTH (`native/xr`, angepasst an SDL2/Wargus):
- Immersive OpenXR-App (Loader-AAR 1.1.58), eigener XR-Thread mit geteiltem GL-Kontext,
  Spielbild per Linker-Wrap an `SDL_RenderPresent` (auch Filme), feste Renderfläche 1280×800.
- Fenster 2 m / 1,8 m vor der Blickrichtung, Griff links + Stick = Breite/Abstand,
  Passthrough optional, Strahl + Zielpunkt, Ladeanzeige und Fehlerseite in der Brille.
- Steuerung nach `docs/BEDIENUNG.md` (Trigger/Griff = Links-/Rechtsklick, Stick = Scrollen,
  A = Umschalt, X = Strg, B = Escape, Y = Pause/Spielmenü), Kanten-Scrollen aus.
- VR-Menü im Wargus-Stil (Panel, Schaltflächen, Schrift und Schriftfarben aus den eigenen
  Spieldaten), Reiter Spiel / Gruppen / Fenster & Bild; Mensch-/Ork-Stil.
- Filme: Überspringen per B oder Menü, „Filme auslassen“ automatisch (Erkennung über die
  YV12-Textur von `movie.cpp`).
- VR-Tastatur über das Menü (Stratagus hält Texteingabe dauerhaft an).
- Desktop-Vorschau der VR-Anzeigen: `tools/menu-preview/build.sh`.

Offen: Headset-Abnahme (Bild, Strahltreffer, Rahmenauswahl, Scrollen, Menü, Filme, Musik),
höhere Renderauflösung (mehr Karte) mit Bestätigung/Rückfall, Kontrollgruppen per Langdruck.

### S4 — Komfort, Paket
Einstellungen weiter ausbauen, Build-Skript/Anleitung für Mitspieler, privater APK-Test.

### War1gus — 🟡 gebaut (0.3.0-xr), Headset-Abnahme offen
Zweite App `xr.wargus1` aus derselben Codebasis (Gradle-Flavor, Spielpaket per App-ID in
`native/xr/xr_game.h`), Datensatz per `scripts/prepare-data-war1.sh` aus `original-data/warcraft1`.
Offen: Start im Headset, Lesbarkeit bei 640×400, eigene App-Icons beider Apps seit 2026-10-03 vorhanden (docs/ICONS.md), Anzeige auf der Quest ungeprüft.

### Später
Tischansicht/Diorama wie Settlers, Mehrspieler über LAN.
