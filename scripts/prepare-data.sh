#!/usr/bin/env bash
# Stellt aus der eigenen Spielkopie den Quest-Datensatz zusammen (nur lokal, nie weitergeben).
#
#   scripts/prepare-data.sh <spielordner> [zielordner]
#     spielordner: installiertes Spiel (Battle.net-Edition/GOG mit INSTALL.MPQ, oder DOS-CD)
#     zielordner:  Standard original-data/wargus-quest
#
# Schritte: Wargus-Skripte/Karten vom Submodul-Stand (passend zur Engine) → wartool -v
# (Grafik, Sound, Karten, Videos; ffmpeg im PATH nötig) → Korrekturen → TiMidity + Freepats
# für die MIDI-Musik. Danach: scripts/quest.sh import-data <zielordner> --replace
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="${1:?Spielordner angeben (enthält INSTALL.MPQ bzw. DATA/)}"
OUT="${2:-$ROOT/original-data/wargus-quest}"
WARGUS="$ROOT/upstream/wargus"
DL="$ROOT/.build/downloads"

FREEPATS_URL="https://deb.debian.org/debian/pool/main/f/freepats/freepats_20060219.orig.tar.gz"
FREEPATS_SHA256="70bf8ca084df3903d6c9de43fe20539fc0a553d95cfba4d525da3fe66fda5f10"

command -v ffmpeg >/dev/null || { echo "ffmpeg fehlt im PATH (Videos)" >&2; exit 1; }

# ---- wartool vom selben Stand wie die Skripte ----
MINGW="$(ls -d "$ROOT"/.build/tools/llvm-mingw-*/bin 2>/dev/null | tail -1 || true)"
[[ -n "$MINGW" ]] && export PATH="$MINGW:$PATH"
SDK="${ANDROID_HOME:-$ROOT/../_tools/android-sdk}"
[[ -d "$SDK/cmake" ]] && export PATH="$(cd "$SDK" && pwd)/cmake/${XR_CMAKE_VERSION:-3.31.6}/bin:$PATH"
WARTOOL="$ROOT/.build/host-wargus/wartool"
[[ -f "$WARTOOL.exe" ]] && WARTOOL="$WARTOOL.exe"
if [[ ! -f "$WARTOOL" ]]; then
	SYSLIBS=""
	[[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* ]] && SYSLIBS="-lshlwapi -lwininet -lole32 -lshell32 -luuid -lcomdlg32"
	cmake -S "$WARGUS" -B "$ROOT/.build/host-wargus" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER="${HOST_CC:-clang}" -DCMAKE_CXX_COMPILER="${HOST_CXX:-clang++}" \
		-DENABLE_VENDORED_LIBS=ON -DSTRATAGUS_INCLUDE_DIR="$ROOT/upstream/stratagus/gameheaders" \
		-DSTRATAGUS=stratagus -DCMAKE_EXE_LINKER_FLAGS="$SYSLIBS"
	# ExternalProjects ohne Byproducts: zuerst bauen, sonst kennt Ninja ihre .a nicht
	cmake --build "$ROOT/.build/host-wargus" --target zlib png bzip2 StormLib
	cmake --build "$ROOT/.build/host-wargus" --target wartool
	WARTOOL="$ROOT/.build/host-wargus/wartool"; [[ -f "$WARTOOL.exe" ]] && WARTOOL="$WARTOOL.exe"
fi

# ---- Grundgerüst wie install() in upstream/wargus/CMakeLists.txt ----
rm -rf "$OUT"
mkdir -p "$OUT/graphics/missiles" "$OUT/graphics/ui/cursors"
cp -r "$WARGUS"/{shaders,contrib,campaigns,maps,scripts} "$OUT/"
cp "$WARGUS/contrib/red_cross.png" "$OUT/graphics/missiles/"
cp "$WARGUS/contrib/cross.png" "$OUT/graphics/ui/cursors/"
( cd "$WARGUS/contrib" && cp black_title.png food.png health.png health2.png mana.png mana2.png \
	"ore,stone,coal.png" score.png "$OUT/graphics/ui/" )

# ---- Extraktion ----
GAME_ABS="$(cd "$GAME" && pwd)"
( cd "$OUT" && "$WARTOOL" -v "$GAME_ABS" . ) > "$OUT.wartool.log" 2>&1 || {
	echo "wartool fehlgeschlagen, siehe $OUT.wartool.log" >&2; exit 1; }

# ---- Korrekturen ----
# wartool (BNE): Musik liegt als .mid vor, ConvertMusic findet keine .wav und trägt dann
# trotzdem ".wav" als Endung ein → keine Musik. Endung nach tatsächlichem Bestand setzen.
CFG="$OUT/scripts/wc2-config.lua"
if ls "$OUT"/music/*.mid >/dev/null 2>&1 && ! ls "$OUT"/music/*.wav >/dev/null 2>&1 \
		&& ! ls "$OUT"/music/*.ogg >/dev/null 2>&1; then
	sed -i 's/^wargus.music_extension = .*/wargus.music_extension = ".mid"/' "$CFG"
fi
# Speichern-Dialog: Datum/Uhrzeit/Karte statt „game.sav“ vorschlagen
bash "$(dirname "$0")/patch-save-name.sh" "$OUT"
# Zufallskarte ins Benutzerverzeichnis (Datenordner ist für die App schreibgeschützt)
bash "$(dirname "$0")/patch-random-map.sh" "$OUT"
# Kopie des Originalarchivs (nur für die Extraktion) nicht mitschleppen
rm -f "$OUT"/install.mpq "$OUT"/INSTALL.MPQ

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

echo "Fertig: $OUT ($(du -sh "$OUT" | cut -f1)); Musik-Endung: $(grep music_extension "$CFG")"
