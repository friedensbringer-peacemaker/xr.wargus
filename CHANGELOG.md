# Changelog xr.stratagus

## Unveröffentlicht

### Eigene Icons für beide Apps (2026-10-03)

- Beide Apps hatten dasselbe SDL-Standard-Icon. Jetzt eigene Launcher-Icons als Flavor-Ressourcen
  (`android/src/{stratagus,war1gus}/res/mipmap-*`), eine Familie auf derselben hellen Kachel:
  `xr.wargus2` blauer Wappenschild mit Silberrand und römischer „II“, `xr.wargus1` weinroter Schild mit
  Goldrand und „I“. Prozedural erzeugt (`tools/launcher-icon/make_icon.py`, keine Fremdgrafik/Spieldaten/
  Marken), Herkunft und Farbwerte in `docs/ICONS.md`. Das SDL-Icon in `main/res` bleibt als Rückfall liegen.

**Nachweis:** gebaut, APK-Gate beide bestanden (SHA-256 `fce06dd4…ac018c22` xr.wargus2,
`98ffbd19…ecb7a404` xr.wargus1), Icons im APK = erzeugte PNGs (MD5 verglichen), Host-Tests 0 Fehler.
Anzeige auf der Quest ungeprüft.

### Daten-Korrektur 2026-10-03 (ohne neue APK)

- **Absturz beim Start einer Zufallskarte** (Warcraft I, Log 07:14; Warcraft II „Random Skirmish“ gleich
  betroffen): Stratagus schreibt `maps/randommap.smp` in den Datenordner, den die App nicht beschreiben darf
  („RawFileWriter: Can't open …/maps/randommap.smp“) → Karte fehlt → Abbruch auf dem Fehlerpfad
  (`FORTIFY: pthread_mutex_lock called on a destroyed mutex`). Fix `scripts/patch-random-map.sh` (in beiden
  prepare-Skripten): Zufallskarte nach `$XRS_USER_DIR/randommap.smp` (absolute Pfade nimmt Stratagus an);
  auf der Quest in beide Datensätze übernommen. Lua-Syntax geprüft, im Headset noch nicht.
- **Warcraft I: Oberfläche bei hoher Auflösung zu klein** (Headset 03.10.: 1280×800 wählbar, UI winzig).
  War1gus zeichnet die Oberfläche in Originalpixeln, das VR-Spielbild ist fest 1280×800 (Nearest). Fix
  `scripts/patch-war1-resolutions.sh` (prepare-data-war1.sh): Optionsmenü bietet nur noch „640x400 (VR large)“
  (2×, scharf), „800x500 (VR medium)“, „960x600 (VR small)“; andere gespeicherte Werte (auch 1280×800) werden
  beim Start auf 640×400 gesetzt (nur auf der Quest). Lua-Test der Prüfung (6 Fälle, PC unverändert); auf
  der Quest übernommen, im Headset offen.

### 0.4.3-xr — 2026-10-03 (Branch `war1gus`)

- **Optionaler Röhrenlook** (Wunsch): Reiter „Anzeige“ → „Bildmodus: normal / Scanlines / CRT“, gespeichert
  (`crt=0..2`, beim Laden geklemmt; „Bild zurücksetzen“ setzt auch ihn zurück). Im Filter-Shader der
  XR-Schicht (`xr_gl.cpp`): Scanlines im 2-Pixel-Raster (Zeile hell / ×0,62, Ausgleich ×1,18; War1gus = eine
  Zeile je Spielpixel), CRT zusätzlich dezente RGB-Streifenmaske (1,06/0,92) und Vignette nach Skill
  xr-image-quality. Keine Wölbung: die Strahl-Abbildung auf das Fenster bleibt exakt.
- Tests auf der Quest erweitert (`tests/run.sh quest`): `test_filter_gpu` (Shader auf der Quest-GPU
  kompiliert/gelinkt, Testbild je Modus gerendert und ausgelesen: Zeilenverhältnis 0,62, Maske, PPM-Ausgabe)
  und `test_music_hook` (Filmton-Fix gegen echte Filme).

**Nachweis:** gebaut (0 Warnungen, Symbolprüfung, APK-Gate beide; SHA-256 `643cd1b8…d858580ea` xr.wargus2,
`71197e99…1dece62f95` xr.wargus1), Host-Tests 0 Fehler, Tests auf der Quest 0 Fehler (inkl. GPU-Filter,
Filmton). Installiert, nicht gestartet.

### 0.4.2-xr — 2026-10-03 (Branch `war1gus`)

Headset 0.4.1 (Nutzer): „Warcraft 2 startet jetzt und läuft super“ – Log 06:45: 60 fps konstant, XR 72 Hz,
nicht gerendert 0, Intro übersprungen, Partie gespielt, Speichern-Dialog geöffnet.
- **Gruppen direkt im Schnellmenü** (Wunsch: einfacher speichern/wählen, schmale Knöpfe im ersten
  Bild): Leiste mit Modus **Wählen / Speichern / Ergänzen** als Auswahl statt Durchschalter (Log: Modus
  mehrfach im Kreis geklickt) und Knöpfen **1–9** (85 × 52 px). Zahl → Aktion, Menü zu, Modus zurück
  auf „Wählen“. Aktiver Modus: helle Schrift + goldene Unterstreichung. Reiter „Gruppen“ entfällt
  (jeder Eintrag einmal), „Eingaben loggen“/„Diagnose“ jetzt im Reiter „Spiel“. Große Knöpfe 50 px hoch.
- **Speichern-Dialog schlägt einen Namen vor:** `JJJJ-MM-TT_hh-mm-Karte.sav` statt immer `game.sav`
  (`scripts/patch-save-name.sh`, von beiden prepare-Skripten angewendet; auf der Quest in beide Datensätze
  übernommen). Lua 5.1-Test mit/ohne Karte.

**Nachweis:** gebaut (0 Warnungen, Symbolprüfung, APK-Gate beide; SHA-256 `e1598deb…69be9be24` xr.wargus2,
`f335ae7c…b458972` xr.wargus1), Host-Tests 0 Fehler (test_ui: Gruppenleiste 3 Modi +
9 Gruppen, aktiver Modus, Stick links/rechts über 1–9, Gruppen nur im Schnellmenü), Vorschau-PNGs Wargus
und War1gus angesehen. Installiert, nicht gestartet.

### 0.4.1-xr — 2026-10-03 (Branch `war1gus`)

- **Fix Startfilm-Hänger (beide Apps).** Hänger-Wächter (Log 03.10. 06:11/06:12): Engine-Thread läuft mit
  100 % CPU in `PlayMovie → PlayMusic → LoadMusic → Mix_LoadMUSType_RW → ov_open (vorbisfile) →
  _open_seekable2 → _get_prev_page_serial`, nach 30 s unverändert. Ursache: Stratagus
  `CFile::to_SDL_RWops` (iolib.cpp) gibt beim Spulen das Ergebnis von `fseek` (0) statt der Position
  zurück → `SDL_RWtell` meldet immer 0 → vorbisfiles Rückwärtssuche nach der letzten Tonseite endet nie.
  Auf Android landet Stratagus auf diesem Weg, weil `Mix_LoadMUS` mit relativem Pfad scheitert
  (SDL öffnet relative Pfade dort nur im internen Speicher/den Assets). Am Gerät nachgestellt
  (`tests/test_movie_audio.cpp broken`: 15-s-Abbruch mit 100 % CPU; korrekte Position: 1 ms).
  Fix ohne Upstream-Änderung (`native/src/music_hook.cpp`, Linker-Wrap): `Mix_LoadMUS` bekommt absolute
  Pfade, `Mix_LoadMUS_RW` eine Datenquelle mit selbst geführter Position.
- Damit war auch das automatische Überspringen unschuldig: Der Hänger lag vor dem ersten Filmbild.

**Nachweis:** gebaut (0 Warnungen, Symbolprüfung, APK-Gate beide; SHA-256 `8e35d22c…7044527a` xr.wargus2,
`15353675…0eeb6b0` xr.wargus1), Host-Tests 0 Fehler. Gerätetest `tests/test_music_hook.cpp` (Positions-Hülle aus
`native/src/tracked_rwops.h`, vorbisfile mit den SDL_mixer-Rückrufen, CFile-artige Quelle) auf der Quest:
alle 13 Wargus-Filme + 2 War1gus-Filme mit Ton in 1–4 ms geöffnet, Position korrekt, 0 Fehler; ohne Hülle
(„roh“) hängt `logo.ogv` weiter (15-s-Abbruch, 100 % CPU). Installiert, nicht im Headset gestartet.

### 0.4.0-xr — 2026-10-03 (Branch `war1gus`)

- **Neue App-IDs/Labels:** `xr.stratagus` → **`xr.wargus2`** (Warcraft II/Wargus), `xr.war1gus` → **`xr.wargus1`**
  (Warcraft I/War1gus). Neue Pakete → auf der Quest neu installiert, Spieldaten aus den alten Apps kopiert.
  Java-Namespace (`xr.stratagus.StratagusActivity`) und Gradle-Flavors bleiben.
- **Startfilm-Hänger (beide Apps), Befund aus den Logs vom 03.10. 05:54/05:55:** Film 1 beginnt, ~0,5–1,5 s
  später liefert der Engine-Thread kein Bild mehr (Statistik: 30 bzw. 90 Bilder in 10 s, alle ~17 ms
  auseinander, danach 0 fps), Escape/Klicks kommen nicht mehr an, „Film läuft“ bleibt 1. Der Engine-Thread
  steht also in `PlayMovie` (Stratagus `movie.cpp`). Ursache noch nicht belegt.
- **Hänger-Wächter** (`native/xr/xr_watchdog.cpp`): liefert der Engine-Thread 3/10/30 s kein Bild, schreibt
  der XR-Thread dessen Aufrufstapel (Signal + Rahmenzeiger-Kette, Bibliothek+Offset/Symbol) und Zustand,
  CPU-Zeit und wchan aller Threads ins Sitzungslog.

**Nachweis:** gebaut (0 Warnungen, Symbolprüfung, APK-Gate beide), Host-Tests 0 Fehler (test_game prüft
die neuen IDs und weist die alten ab). Installiert, nicht gestartet.

### 0.3.1-xr — 2026-10-03 (Branch `war1gus`)

- **Filme automatisch überspringen** feuerte schon beim Anlegen der Filmtextur – Stratagus startet in dem
  Moment erst die Filmmusik und setzt die Film-Tastenbehandlung danach. Verdacht (Nutzer): Hänger beim
  Start mit „Filme auslassen: an“. Jetzt erst nach 0,8 s laufendem Film, bei Bedarf bis zu dreimal im
  Abstand von 1 s; nie, wenn kein Film läuft.
- Logs (aus 0.2.5): Log-Ordner ohne Setgid wird beim Start neu angelegt, alte Logs hineinkopiert – sie
  werden so für adb lesbar.

**Nachweis:** gebaut (0 Warnungen, Symbolprüfung, APK-Gate), Host-Tests 0 Fehler. Ursache des Hängers
nicht belegt (Logs der Tests vom 02.10. unlesbar, logcat überschrieben).

### 0.3.0-xr — 2026-10-02 (Branch `war1gus`)

Zweite App **`xr.war1gus`** (War1gus = Warcraft-I-Spielpaket der Stratagus-Engine) aus derselben
Engine- und XR-Schicht (NAME-001):
- **Submodul** `upstream/war1gus` (master `8ec75764c`, 2026-06-22). Passt zur Engine: Stratagus `3d87c93f7`
  ist ebenfalls master vom 2026-06-22 (kein neuerer Engine-Commit), War1gus wird upstream gegen
  Stratagus-master gebaut; keine Skriptänderung seit dem Engine-Stand.
- **Gradle-Flavors** `stratagus` (App-ID/Label `xr.stratagus`) und `war1gus` (`xr.war1gus`), gleiche
  `libmain.so`. Die Activity übergibt die App-ID (`--xr-app=…`), `native/xr/xr_game.h` wählt das
  Spielpaket: Datenordner `data.Wargus`/`data.War1gus`, Einstellungen `wc2`/`wc1`, VR-Voreinstellungen,
  Menügrafiken, Schrift, Texte der Lade-/Fehlerseite.
- **War1gus-Voreinstellungen** (Marker `xr-defaults-v1`): 640×400 (= Renderfläche 1280×800 / 2, scharf,
  ohne Balken), `OriginalPixelRatio` aus.
- **VR-Menü War1gus:** Panel aus den Spieldaten (`ui/<volk>/panel_1.png`), Schrift `contrib/fonts/large.png`
  (14 px), Schaltflächen gezeichnet (War1gus hat keine Knopfgrafiken). Beschriftungen, die in der
  breiteren Schrift nicht passen, werden bis 80 % gestaucht (bei Wargus nie nötig).
- **Datensatz:** `scripts/prepare-data-war1.sh` baut `war1tool` (llvm-mingw; `realpath`-Abbildung per
  Zwangs-Include, Submodul unverändert) und erzeugt `original-data/war1gus-quest` (Skripte/Karten wie
  `install()`, `war1tool -v`, MIDI + TiMidity/Freepats). Korrektur: Kanten-Scrollen aus
  (`SetMouseScroll(false)`; in War1gus fest an, keine Einstellung).
- `scripts/quest.sh`: `PKG=xr.war1gus` (APK, Datenordner, Prüfdateien; Datensätze werden der
  falschen App verweigert). APK-Pfade jetzt `android/build/outputs/apk/<flavor>/debug/`.
- xr.stratagus unverändert: gleiche Voreinstellungen/Pfade (Host-Test `test_game`), VR-Anzeigen
  bytegleich zum Stand 0.2.5 (PNG-Vergleich), nur die Logzeile `theme:` nennt jetzt Panel/Knöpfe getrennt.

**Nachweis:** gebaut (native 0 Warnungen, Symbolprüfung; APK-Gate beide bestanden, SHA-256
`a087da8a…5cafb4` xr.stratagus, `8439aa39…e654c1` xr.war1gus; sauberer Neubau aus dem Commit-Stand), Host-Tests 107 293 / 2 182 / 143 /
7 447 (Wargus) / 6 953 (War1gus) Prüfungen, 0 Fehler. Datensatz erzeugt (93 MB, 34 Videos). Nicht
installiert, nicht im Headset gestartet, War1gus auch am PC nicht gestartet.

## 0.2.4-xr — 2026-10-02 (Branch `xr-menu-pflicht`)

VR-Menü auf den Pflichtumfang gebracht (Skill xr-vr-menu §0, P1–P7):
- **Zwei Stufen (P5):** Schnellmenü mit 8 großen Knöpfen (Weiterspielen, Fenster zentrieren,
  Passthrough, Spielmenü, Tastatur, Einstellungen, Beenden, Schließen); Einstellungen mit den Reitern
  Spiel / Gruppen / Fenster / Anzeige, jede Seite endet mit „Zurück“ und „Schließen“. Das Menü öffnet
  immer im Schnellmenü. Ein Raster für alles: `native/xr/xr_menu_layout.h`.
- **Beenden (P3):** rot mit hellrotem Rand; erster Klick schaltet sichtbar scharf („Sicher? Nochmal
  klicken“, gefüllt rot, Doppelpuls), zweiter Klick innerhalb 3 s speichert die Einstellungen und
  schließt die App (`finishAndRemoveTask`, Rückfall `SDL_QUIT`). Nach 3 s, bei anderem Klick oder beim
  Schließen des Menüs fällt der Knopf zurück.
- **Spiel-Mauszeiger: an/aus (P3/P4):** blendet den Wargus-Mauszeiger aus (Wrap von `DrawCursor`/
  `HideCursor`; Auswahlrahmen und Gebäude-Umriss bleiben), gespeichert als `game_cursor`.
- **Sättigung** als Bildregler; Schalter zeigen ihren Zustand nur im Text, nicht mehr als „gedrückt“.
- **Stick/A/B (P2):** Stick bewegt einen goldenen Fokusrahmen (bleibt in Zeile/Spalte, überspringt
  Gesperrtes, wiederholt bei Dauerdruck), A löst aus, B/Menütaste schließen. Der Strahl übernimmt den
  Fokus, sobald er eine andere Schaltfläche trifft. Größe/Abstand/Bildregler/Tempo wiederholen bei
  gehaltenem Trigger bzw. A. Ein im Menü gedrücktes A wird nach dem Schließen erst nach Loslassen
  wieder zu Umschalt.
- Logik in `native/xr/xr_menu_model.h` (ConfirmAction, PointerVisibility, HoldRepeat) – Kandidat für
  `xrc/menu.h` (xr-common), übertragbar auf XR-0AD.

**Nachweis:** gebaut (0 Warnungen, Symbolprüfung, APK-Gate, SHA-256 `7c6b99cc…9b72491`), Host-Tests
107 293 / 2 182 / 6 952 Prüfungen, 0 Fehler; Vorschau-PNGs angesehen. Nicht installiert, nicht im
Headset gestartet.

## 0.2.3-xr — 2026-10-02

- Headset 0.2.2: „das Menü links konnte geladen werden, aber das Spiel selbst hing fest“.
  Sitzungslog dazu auf dem Gerät nicht lesbar (s. u.), daher Ursache nicht belegt. Wahrscheinlich:
  Stratagus pausiert bei `SDL_WINDOWEVENT_FOCUS_LOST` das Spiel (`PauseOnLeave`); die immersive App hat
  aus Android-Sicht oft keinen Fensterfokus. Fix: Fokus- und Enter/Leave-Ereignisse des
  Android-Fensters werden verworfen (`android_main.cpp`), `PauseOnLeave = false` als VR-Voreinstellung
  (Marker `xr-defaults-v3`, ältere Voreinstellungen werden nicht erneut überschrieben).
- Logs: 0.2.2 setzte den Log-Ordner auf 0770 und verlor dabei das Setgid-Bit → neue Logs gehörten der
  App-Gruppe und waren für adb unlesbar. Jetzt 02770; vorhandene unlesbare Logs werden beim Start neu
  geschrieben (so wird auch das Log des Tests vom 01.10. 23:50 lesbar).
- Belegt aus dem Log vom 29.09.: SDL-Kontext ist OpenGL ES 3.2 (EGL-Client 3).

**Nachweis:** gebaut (0 Warnungen, Symbolprüfung, APK-Gate, SHA-256 `14929c51…a82402b`), Tests auf der
Quest bestanden, installiert (`dumpsys`: 0.2.3-xr). Nicht im Headset gestartet.

## 0.2.2-xr — 2026-09-30

- **Fix Absturz beim Start (0.2.0/0.2.1):** Stratagus `src/video/shaders.cpp` definiert globale
  Funktionszeiger mit den Namen echter GL-Funktionen (`glUseProgram`, `glCreateShader`, … 20 Stück).
  In `libmain.so` überdeckten sie die Funktionen aus libGLESv3; der erste GL-Aufruf der XR-Schicht
  (`GlStateGuard`, beim ersten Frame) sprang in `.bss` → SIGSEGV. Ursache → Fix: nur für
  `shaders.cpp` per Präprozessor umbenannt (`native/CMakeLists.txt`), Upstream unverändert.
- Build prüft jetzt, dass `libmain.so` keine Variablen unter GL-/EGL-/OpenXR-Funktionsnamen enthält.
- Sitzungslogs (`files/logs`) für adb lesbar (`scripts/quest.sh logs`).

**Nachweis:** gebaut (0 Warnungen, Symbolprüfung, APK-Gate bestanden, SHA-256 `622fd27f…776dc96`),
Tests am PC und auf der Quest (ARM64) bestanden, installiert (`dumpsys`: 0.2.2-xr). Nicht im Headset
gestartet. Die Tests konnten den Fehler nicht finden, weil sie Stratagus nicht mitlinken.

## 0.2.1-xr — 2026-09-30

- QS: Fenster-Geometrie (Pose, Grenzen, Strahltreffer) nach `native/xr/xr_geometry.h` ausgelagert
  und per Host-Test geprüft; Tests `tests/test_geometry.cpp`, `test_input.cpp`, `test_ui.cpp`,
  Aufruf `tests/run.sh host|quest`.
- QS: `scripts/apk-gate.sh` (Manifest, Libs, Signatur, keine Spieldaten), zentrale Version in
  `version.properties`, STATUS/BACKLOG/TEST.md, Prüfprotokoll `docs/VALIDATION.md`.
- Fix: fehlendes `#include <cstdlib>` in `xr_canvas.cpp` (fiel erst im Desktop-Build auf).
- VR-Menü: aktiver Reiter und eingeschaltete Optionen mit weißer Schrift, Eintrag unter dem Strahl
  mit goldenem Rahmen (das Wargus-Bild „gedrückt“ allein war kaum zu unterscheiden).

**Nachweis:** gebaut (native warnungsfrei, APK-Gate bestanden, SHA-256 `cf90d0f0…3b6eaa`),
Host-Tests bestanden (siehe `docs/VALIDATION.md`). Installiert/im Headset geprüft: siehe VALIDATION.
Verhalten sonst wie 0.2.0-xr.

## 0.2.0-xr — 2026-09-29

- Immersive OpenXR-App: Spielbild als Fenster (Quad) im Raum, eigener XR-Thread, Strahl, Passthrough,
  Ladeanzeige/Fehlerseite in der Brille (XR-Schicht aus XR-CorsixTH, angepasst an SDL2/Wargus).
- Controller-Steuerung für Warcraft II (`docs/BEDIENUNG.md`), Kanten-Scrollen aus.
- VR-Menü im Wargus-Stil (Grafiken/Schrift aus den eigenen Spieldaten), VR-Tastatur, Filme überspringen.

**Nachweis:** gebaut und installiert (2026-09-29), nie im Headset gestartet.

## 0.1.0-s1 — 2026-09-27

- Flache Android-App (Panel): Stratagus/Wargus startet auf der Quest bis ins Menü.
- Musik (TiMidity + Freepats), Film-Ton, 1280×800, sauberes Beenden.

**Nachweis:** Hauptmenü im Headset gesehen (Screenshot 2026-09-27); Musik/Film-Ton ungeprüft.
