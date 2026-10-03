#!/usr/bin/env bash
# Stellt aus der eigenen Warcraft-I-Kopie den Quest-Datensatz für xr.wargus1 zusammen
# (nur lokal, nie weitergeben). Gegenstück zu scripts/prepare-data.sh (Wargus).
#
#   scripts/prepare-data-war1.sh <spielordner> [zielordner]
#     spielordner: installiertes Spiel mit DATA/DATA.WAR (GOG/DOS-CD)
#     zielordner:  Standard original-data/war1gus-quest
#
# Schritte: War1gus-Skripte/Karten vom Submodul-Stand (wie install() in
# upstream/war1gus/CMakeLists.txt: campaigns contrib maps scripts shaders) → war1tool -v
# (Grafik, Sound, Karten, Videos; ffmpeg im PATH nötig) → Korrekturen → TiMidity + Freepats für
# die MIDI-Musik. Danach: PKG=xr.wargus1 scripts/quest.sh import-data <zielordner> --replace
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="${1:?Spielordner angeben (enthält DATA/DATA.WAR)}"
OUT="${2:-$ROOT/original-data/war1gus-quest}"
WAR1GUS="$ROOT/upstream/war1gus"
DL="$ROOT/.build/downloads"

FREEPATS_URL="https://deb.debian.org/debian/pool/main/f/freepats/freepats_20060219.orig.tar.gz"
FREEPATS_SHA256="70bf8ca084df3903d6c9de43fe20539fc0a553d95cfba4d525da3fe66fda5f10"

command -v ffmpeg >/dev/null || { echo "ffmpeg fehlt im PATH (Videos)" >&2; exit 1; }

# ---- war1tool vom selben Stand wie die Skripte ----
MINGW="$(ls -d "$ROOT"/.build/tools/llvm-mingw-*/bin 2>/dev/null | tail -1 || true)"
[[ -n "$MINGW" ]] && export PATH="$MINGW:$PATH"
SDK="${ANDROID_HOME:-$ROOT/../_tools/android-sdk}"
[[ -d "$SDK/cmake" ]] && export PATH="$(cd "$SDK" && pwd)/cmake/${XR_CMAKE_VERSION:-3.31.6}/bin:$PATH"
WAR1TOOL="$ROOT/.build/host-war1gus/war1tool"
[[ -f "$WAR1TOOL.exe" ]] && WAR1TOOL="$WAR1TOOL.exe"
if [[ ! -f "$WAR1TOOL" ]]; then
	CXXFLAGS_EXTRA="" SYSLIBS=""
	if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* ]]; then
		SYSLIBS="-lshlwapi -lwininet -lole32 -lshell32 -luuid -lcomdlg32"
		# war1tool.cpp ersetzt realpath() nur für MSVC; llvm-mingw kennt es nicht → gleiche Abbildung
		# auf _fullpath per Zwangs-Include (Submodul bleibt unverändert).
		COMPAT="$ROOT/.build/host-war1gus-compat.h"
		mkdir -p "$ROOT/.build"
		printf '#pragma once\n#include <stdlib.h>\n#define realpath(N, R) _fullpath((R), (N), _MAX_PATH)\n' > "$COMPAT"
		CXXFLAGS_EXTRA="-include $(cygpath -m "$COMPAT")"
	fi
	cmake -S "$WAR1GUS" -B "$ROOT/.build/host-war1gus" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER="${HOST_CC:-clang}" -DCMAKE_CXX_COMPILER="${HOST_CXX:-clang++}" \
		-DCMAKE_CXX_FLAGS="$CXXFLAGS_EXTRA" -DCMAKE_EXE_LINKER_FLAGS="$SYSLIBS" \
		-DENABLE_VENDORED_LIBS=ON -DSTRATAGUS_INCLUDE_DIR="$ROOT/upstream/stratagus/gameheaders" \
		-DSTRATAGUS=stratagus
	# ExternalProjects ohne Byproducts: zuerst bauen, sonst kennt Ninja ihre .a nicht
	cmake --build "$ROOT/.build/host-war1gus" --target zlib png
	cmake --build "$ROOT/.build/host-war1gus" --target war1tool
	WAR1TOOL="$ROOT/.build/host-war1gus/war1tool"; [[ -f "$WAR1TOOL.exe" ]] && WAR1TOOL="$WAR1TOOL.exe"
fi

# ---- Grundgerüst wie install() in upstream/war1gus/CMakeLists.txt ----
rm -rf "$OUT"
mkdir -p "$OUT"
cp -r "$WAR1GUS"/{campaigns,contrib,maps,scripts,shaders} "$OUT/"

# ---- Extraktion ----
# war1tool wechselt unter Windows in sein eigenes Verzeichnis (_wchdir) → absolute Pfade.
GAME_ABS="$(cd "$GAME" && pwd)"
OUT_ABS="$(cd "$OUT" && pwd)"
if command -v cygpath >/dev/null 2>&1; then GAME_ABS="$(cygpath -m "$GAME_ABS")"; OUT_ABS="$(cygpath -m "$OUT_ABS")"; fi
"$WAR1TOOL" -v "$GAME_ABS" "$OUT_ABS" > "$OUT.war1tool.log" 2>&1 || {
	echo "war1tool fehlgeschlagen, siehe $OUT.war1tool.log" >&2; exit 1; }
[[ -f "$OUT/extracted" && -f "$OUT/scripts/wc1-config.lua" ]] || {
	echo "war1tool lief, aber extracted/wc1-config.lua fehlen – siehe $OUT.war1tool.log" >&2; exit 1; }

# ---- Korrekturen ----
# Musik: ohne -m bleibt sie MIDI (.mid, wc1-config.lua sagt das schon). Gespielt über TiMidity
# (unten); kontrollieren, dass die Endung zum Bestand passt.
CFG="$OUT/scripts/wc1-config.lua"
if ls "$OUT"/music/*.mid >/dev/null 2>&1 && ! grep -q '".mid"' "$CFG"; then
	sed -i 's/^war1gus.music_extension = .*/war1gus.music_extension = ".mid"/' "$CFG"
fi
# Kanten-Scrollen: War1gus schaltet es fest ein (SetMouseScroll(true) in scripts/stratagus.lua,
# keine Einstellung). In VR verlässt der Strahl das Fenster ständig → der auf den Rand
# geklemmte Zeiger würde die Karte dauernd schieben. Gescrollt wird per Stick (Pfeiltasten).
sed -i 's/^SetMouseScroll(true)/SetMouseScroll(false) -- xr.wargus1: Scrollen per Stick/' "$OUT/scripts/stratagus.lua"
# Speichern-Dialog: Datum/Uhrzeit/Karte statt „game.sav“ vorschlagen
bash "$(dirname "$0")/patch-save-name.sh" "$OUT"
# Zufallskarte ins Benutzerverzeichnis (Datenordner ist für die App schreibgeschützt)
bash "$(dirname "$0")/patch-random-map.sh" "$OUT"
# VR: nur Auflösungen mit lesbarer Oberfläche (640×400, 800×500, 960×600)
bash "$(dirname "$0")/patch-war1-resolutions.sh" "$OUT"
grep -q '^SetMouseScroll(false)' "$OUT/scripts/stratagus.lua" || {
	echo "Korrektur Kanten-Scrollen nicht anwendbar (scripts/stratagus.lua geändert?)" >&2; exit 1; }
# Nur für die Extraktion nötige Originalarchive/Zwischendateien nicht mitschleppen
find "$OUT" \( -iname '*.war' -o -iname '*.avi' \) -delete

# ---- MIDI: TiMidity-Konfiguration + Freepats (GPL-2) ----
mkdir -p "$DL"
TGZ="$DL/freepats_20060219.orig.tar.gz"
[[ -f "$TGZ" ]] || curl -sSLf --retry 3 -o "$TGZ" "$FREEPATS_URL"
echo "$FREEPATS_SHA256  $TGZ" | sha256sum -c --quiet -
TMP="$ROOT/.build/freepats-tmp"
rm -rf "$TMP" && mkdir -p "$TMP"
tar -xzf "$TGZ" -C "$TMP"
tar -xjf "$TMP"/freepats-20060219/upstream/freepats-20060219.tar.bz2 -C "$TMP"
mkdir -p "$OUT/timidity"
cp -r "$TMP"/freepats/{Tone_000,Drum_000,COPYING,README} "$OUT/timidity/"
# Stratagus setzt TIMIDITY_CFG auf <Daten>/timidity/timidity.cfg; TiMidity löst Patch-Pfade
# relativ zu dieser Datei auf.
cp "$TMP"/freepats/freepats.cfg "$OUT/timidity/timidity.cfg"
rm -rf "$TMP"

echo "Fertig: $OUT ($(du -sh "$OUT" | cut -f1)); $(cat "$CFG"); Videos: $(ls "$OUT"/videos/*.ogv 2>/dev/null | wc -l)"
