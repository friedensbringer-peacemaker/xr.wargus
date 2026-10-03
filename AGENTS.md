# XR-Stratagus – Arbeitskontext für Coding-Agenten

Meta-Quest-3-Port der Open-Source-RTS-Engine **Stratagus** mit dem Spielpaket **Wargus**
(klassisches Orcs-vs-Humans-RTS von 1995/96). Das Spiel läuft als 2D-Bild auf einem
OpenXR-Quad-Layer; Controller ersetzen Maus und Tastatur. Vorbild für Architektur und
Werkzeuge ist `../XR-Settlers2.5` (gleiches Muster: SDL-Android-App + engine-interner
OpenXR-Treiber). Plattform-Regeln: `../XR-Ports-Platform/XR-Ports-Platform/AGENTS.md`.

Lies danach [docs/ROADMAP.md](docs/ROADMAP.md) (Schritte, Stand, technische Befunde).

## Harter Rahmen

1. **Keine Originalspieldaten committen oder weitergeben** — weder GOG-Installer noch
   `.war/.mpq` noch von `wartool` extrahierte PNG/WAV/OGV/Karten. Alles liegt nur lokal
   unter `original-data/` (gitignored). Der Spieler importiert seine eigene Kopie.
2. **Keine Markennamen** in App-Name, Paket-ID, Icons oder Store-Texten („Warcraft",
   „Blizzard"). Upstream-Namen Stratagus/Wargus sind ok.
3. Keine Kopierschutz-Umgehung. `wartool` liest nur legal erworbene Daten.
4. Upstream-Code bleibt GPL-2.0; Herkunft und Lizenzen erhalten.
5. Behauptete Headset-Tests müssen tatsächlich durchgeführt sein.
6. Subagents committen, pushen nur der Parent-Agent nach Prüfung.

## Layout

```
upstream/stratagus   Submodul Wargus/stratagus (Engine, C++17, CMake, SDL2, Lua 5.1)
upstream/wargus      Submodul Wargus/wargus (Lua-Skripte, wartool-Extraktor)
upstream/war1gus     Submodul Wargus/war1gus (Warcraft-I-Paket, war1tool) → zweite App xr.wargus1
  */third-party      verschachteltes Submodul Wargus/dependencies (SDL2, Lua, guichan …)
docs/ROADMAP.md      Schritte zum Quest-Port, Befunde
original-data/       LOKAL: GOG-Installer, installierte Spiele, extrahierte Desktop-Testinstallation
release/             LOKAL: heruntergeladene Upstream-Windows-Releases v3.3.2
```

Sobald Engine-Code geändert wird, wird `upstream/stratagus` auf einen eigenen Fork
umgestellt (oder wie bei Settlers als eingebetteter Quellstand übernommen) — nie
Änderungen in einem Submodul auf fremdem Remote liegen lassen.

## Toolchain (Windows-Rechner)

Git Bash + PowerShell. JDK 17 (`JAVA_HOME` gesetzt, Java 8 im PATH!). Gemeinsames Android SDK
aller XR-Projekte: `<XR-Ordner>/_tools/android-sdk` (NDK 30.0.16248370, CMake 3.31.6) —
**nicht** `%LOCALAPPDATA%` (Claude Desktop/MSIX leitet das in eine Sandbox um). Die Skripte
nehmen `ANDROID_HOME`, sonst `../_tools/android-sdk`. `7z` und `ffmpeg` via Chocolatey im PATH
(ffmpeg braucht `wartool` für Videos). Gradle nur über Wrapper. Details: [docs/BUILD.md](docs/BUILD.md).

## Quest-Tests

Nicht ungefragt Apps auf der Quest starten, während der Nutzer das Headset trägt: erst lokal
fertig bauen/prüfen, dann den Test ankündigen. Logger/Screenshots: `<XR-Ordner>/_tools/quest`.
App-Labels `xr.wargus2` und `xr.wargus1` (Konvention `xr.<name>`, kein Markenname; Gradle-Flavors
`stratagus`/`war1gus`, Spielpaket-Unterschiede nur in `native/xr/xr_game.h`). Zweite App: `PKG=xr.wargus1 scripts/quest.sh …`.

## Lokale Desktop-Referenz

`original-data/wargus-desktop/` = Windows-Release 3.3.2 + mit `wartool -v` aus der
GOG-Battle.net-Edition extrahierte Daten. Start: `stratagus.exe -d .` im Ordner.
Dient als Vergleich („so muss es aussehen") und als Datenquelle für den Quest-Import.
