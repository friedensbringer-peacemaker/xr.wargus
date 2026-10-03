# Quest-Build (S1, Windows erprobt)

Voraussetzungen: JDK 17 (`JAVA_HOME`), gemeinsames Android SDK `<XR-Ordner>/_tools/android-sdk`
(`ANDROID_HOME`; NDK 30.0.16248370, **CMake 3.31.6** — Stratagus verlangt ≥ 3.25), Git Bash,
`ffmpeg` im PATH. Nicht das SDK unter `%LOCALAPPDATA%` verwenden (Claude-Desktop-Sandbox).
Host-Compiler für tolua++ und wartool: llvm-mingw, portabel entpackt nach
`.build/tools/llvm-mingw-*/` (Release von github.com/mstorsjo/llvm-mingw, `ucrt-x86_64.zip`).
Auf macOS genügt der System-Clang.

```bash
git submodule update --init --recursive
bash scripts/build-native.sh                       # → android/src/main/jniLibs/arm64-v8a
cd android && ./gradlew assembleDebug; cd ..        # beide Apps (Flavors), einzeln: assembleStratagusDebug / assembleWar1gusDebug
                                                    # → android/build/outputs/apk/stratagus/debug/XR-Stratagus-debug.apk
                                                    # → android/build/outputs/apk/war1gus/debug/XR-War1gus-debug.apk
bash scripts/prepare-data.sh original-data/warcraft2  # eigene Spielkopie → original-data/wargus-quest
bash scripts/quest.sh install
bash scripts/quest.sh import-data original-data/wargus-quest [--replace]
bash scripts/quest.sh start      # nur nach Absprache, wenn der Nutzer das Headset trägt
bash scripts/quest.sh log
bash scripts/apk-gate.sh android/build/outputs/apk/stratagus/debug/XR-Stratagus-debug.apk xr.wargus2 \
  libmain.so libSDL2.so libc++_shared.so libopenxr_loader.so
```

Zweite App `xr.wargus1` (gleiche `libmain.so`, Spielpaket per App-ID, `native/xr/xr_game.h`):

```bash
bash scripts/prepare-data-war1.sh original-data/warcraft1   # → original-data/war1gus-quest
PKG=xr.wargus1 bash scripts/quest.sh install
PKG=xr.wargus1 bash scripts/quest.sh import-data original-data/war1gus-quest [--replace]
bash scripts/apk-gate.sh android/build/outputs/apk/war1gus/debug/XR-War1gus-debug.apk xr.wargus1 \
  libmain.so libSDL2.so libc++_shared.so libopenxr_loader.so
```

## Spieldaten für die Quest (lokal, nie committen)

`scripts/prepare-data.sh` baut bei Bedarf `wartool` vom Submodul-Stand (Engine-`master` braucht
die Wargus-Skripte vom selben Stand, nicht das 3.3.2-Release), kopiert Skripte/Karten wie
`install()` in `upstream/wargus/CMakeLists.txt`, extrahiert mit `wartool -v` und korrigiert:

- **Musikendung:** wartool extrahiert die Battle.net-Musik als `.mid`, schreibt aber
  `wargus.music_extension = ".wav"` in `scripts/wc2-config.lua` → keine Musik. Wird auf `.mid` gesetzt.
- **MIDI-Klänge:** `timidity/` mit Freepats 20060219 (GPL-2, Debian-orig-Tarball, SHA-256 geprüft)
  und `timidity.cfg`; Stratagus setzt `TIMIDITY_CFG` auf `<Daten>/timidity/timidity.cfg`.
- `install.mpq` (Kopie des Originalarchivs) wird entfernt.

## War1gus-Datensatz (`scripts/prepare-data-war1.sh`)

Baut `war1tool` vom Submodul-Stand (`.build/host-war1gus`, llvm-mingw; `war1tool.cpp` bildet
`realpath` nur für MSVC ab → Zwangs-Include `.build/host-war1gus-compat.h`, dazu die Windows-Systembibliotheken
wie bei wartool), kopiert `campaigns contrib maps scripts shaders` wie `install()` in
`upstream/war1gus/CMakeLists.txt`, extrahiert mit `war1tool -v` (ffmpeg für die Videos; die vier
Intro-Teile werden zu `hintro/ointro/cave/title.ogv` zusammengesetzt) und korrigiert:

- **Kanten-Scrollen aus:** War1gus setzt `SetMouseScroll(true)` fest in `scripts/stratagus.lua` (keine
  Einstellung). In VR liegt der Zeiger beim Verlassen des Fensters am Rand → Dauerscrollen. Wird auf
  `false` gesetzt; `quest.sh import-data` verweigert Datensätze ohne diese Korrektur.
- **Musik:** ohne `-m` bleibt sie MIDI (`wc1-config.lua`: `.mid`), gespielt über TiMidity + Freepats wie Wargus.
- Von `war1tool` mitkopierte Originalarchive (`*.WAR`) und Zwischendateien (`*.avi`) werden entfernt.

Ergebnis 2026-10-02 aus der GOG-Kopie: 93 MB (davon Freepats 33 MB, Videos 48 MB, 34 Filme).
Erster Start setzt `user/wc1/preferences.lua`: 640×400, `OriginalPixelRatio = false`.

## Desktop-Gegenprobe (Windows, gleicher Engine-Stand)

Prüft Datensatz und Skripte ohne Headset. Mit llvm-mingw im PATH:

```bash
cmake -S upstream/stratagus -B .build/host-stratagus -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_VENDORED_LUA=ON \
  -DBUILD_VENDORED_SDL=ON -DBUILD_VENDORED_MEDIA_LIBS=ON -DWITH_MNG=OFF -DWITH_OPENMP=OFF \
  -DWITH_STACKTRACE=OFF -DENABLE_STDIO_REDIRECT=OFF -DCMAKE_CXX_FLAGS="-include iterator -Wno-c++11-narrowing"
cmake --build .build/host-stratagus --target stratagus_lib   # ggf. wiederholen (Lua-Header-Race)
cmake --build .build/host-stratagus --target stratagus
SDL_AUDIODRIVER=directsound .build/host-stratagus/stratagus.exe -d original-data/wargus-quest -u .build/host-user
```

`SDL_AUDIODRIVER` muss gesetzt sein: `sound_server.cpp:125` (nur Windows) ruft
`strcmp(SDL_GetHint("SDL_AUDIODRIVER"), …)` ohne NULL-Prüfung → Absturz beim ersten
Musikstück (Intro). Upstream-Bug; betrifft Android nicht. Ergebnis 2026-09-27: Intro-Video läuft.

## Stolpersteine (alle im Build berücksichtigt)

- Vendored SDL 2.0.22 kompiliert nicht mit NDK ≥ 27 (`ALooper_pollAll` entfernt) → eigene
  Submodule `external/SDL` 2.32.10, `SDL_image` 2.8.12, `SDL_mixer` 2.8.2.
- Stratagus-ExternalProjects bekommen nur `CMAKE_TOOLCHAIN_FILE` → Wrapper
  `cmake/quest-arm64.toolchain.cmake` legt ABI/API fest (sonst armv7/API 21).
- Ninja kennt die `.a`-Ausgaben der ExternalProjects nicht → erst `stratagus_lib`, dann `main`.
- Upstream-Lua kopiert `lua.h` erst nach dem Link → parallele tolua++-Übersetzung scheitert
  beim ersten Lauf; Skript wiederholt einmal.
- Bionic-FORTIFY bricht Lua 5.1 in `lgc.c` ab → `-U_FORTIFY_SOURCE` nur für Lua.
- Git Bash schreibt `/sdcard/…` in Windows-Pfade um → `MSYS_NO_PATHCONV=1` in `quest.sh`.
- SDL_mixer mit `stb_vorbis` liest keine Ogg-Dateien mit Theora-Spur → Videos ohne Ton;
  daher `SDL2MIXER_VORBIS=VORBISFILE` mit dessen vendored ogg/vorbis (statisch, in der Linkliste).
- Panel schließen → Android wartet 10 s auf das Ende (`destroyTimeout`), weil Stratagus nur auf
  `SDL_QUIT` reagiert → `android_main.cpp` beendet bei `SDL_APP_TERMINATING` sofort.
- Wargus startet mit 640×480 (Balken im 1280×800-Panel) → beim ersten Start wird
  `user/wc2/preferences.lua` mit 1280×800 vorbelegt; spätere Menü-Änderungen bleiben erhalten.
