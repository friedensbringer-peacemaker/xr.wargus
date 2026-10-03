#!/usr/bin/env bash
# Quest-Hilfen: APK installieren, Spieldaten importieren, starten, Log.
#
#   scripts/quest.sh install              # android/build/outputs/apk/stratagus/debug/XR-Stratagus-debug.apk
#   scripts/quest.sh import-data <ordner> # z. B. original-data/wargus-quest (Wargus-Skripte + wartool-Ausgabe)
#   scripts/quest.sh start | stop | log (logcat live) | logs (Sitzungslogs holen)
#   PKG=xr.wargus1 scripts/quest.sh …     # zweite App (War1gus): XR-War1gus-debug.apk, war1gus-quest
#
# Mehrere Geräte: ANDROID_SERIAL setzen. Daten landen im app-eigenen Speicher und werden nur
# lokal per USB übertragen — nie weitergeben.
set -euo pipefail
# Git Bash würde Gerätepfade wie /sdcard/… sonst in Windows-Pfade umschreiben
export MSYS_NO_PATHCONV=1
to_native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Ein adb für alle XR-Projekte (verschiedene Versionen beenden sich gegenseitig den Server)
SHARED_ADB="$ROOT/../_tools/android-sdk/platform-tools/adb.exe"
[[ -x "$SHARED_ADB" ]] || SHARED_ADB="$ROOT/../_tools/android-sdk/platform-tools/adb"
ADB="${ADB:-$( [[ -x "$SHARED_ADB" ]] && echo "$SHARED_ADB" || echo adb)}"
adb() { "$ADB" "$@"; }
PKG="${PKG:-xr.wargus2}"
case "$PKG" in
	xr.wargus2) FLAVOR=stratagus; APK_NAME=XR-Stratagus-debug.apk; DATA_DIR=data.Wargus; GAME=Wargus
		CHECK_FILES=(scripts/stratagus.lua extracted) ;;
	xr.wargus1)   FLAVOR=war1gus;   APK_NAME=XR-War1gus-debug.apk;   DATA_DIR=data.War1gus; GAME=War1gus
		CHECK_FILES=(scripts/stratagus.lua extracted war1data scripts/wc1-config.lua) ;;
	*) echo "Unbekanntes Paket $PKG (xr.wargus2 oder xr.wargus1)" >&2; exit 1 ;;
esac
# Activity-Klasse liegt für beide Apps im Namespace xr.stratagus
ACTIVITY="$PKG/xr.stratagus.StratagusActivity"
DEST="/sdcard/Android/data/$PKG/files/$DATA_DIR"

case "${1:-}" in
	install)
		adb install -r "$(to_native "$ROOT/android/build/outputs/apk/$FLAVOR/debug/$APK_NAME")"
		;;
	import-data)
		SRC="${2:?Datenordner angeben}"
		for f in "${CHECK_FILES[@]}"; do
			[[ -f "$SRC/$f" ]] || { echo "$SRC ist kein extrahierter $GAME-Datenordner ($f fehlt)" >&2; exit 1; }
		done
		if [[ "$PKG" == xr.wargus1 ]]; then
			# XR-Korrektur aus scripts/prepare-data-war1.sh (Kanten-Scrollen aus), sonst schiebt der
			# auf den Fensterrand geklemmte Strahl-Zeiger die Karte dauernd
			grep -q '^SetMouseScroll(false)' "$SRC/scripts/stratagus.lua" || {
				echo "$SRC nicht mit scripts/prepare-data-war1.sh erzeugt (Kanten-Scrollen noch an)" >&2; exit 1; }
		elif [[ -f "$SRC/war1data" ]]; then
			echo "$SRC ist ein War1gus-Datensatz – für die zweite App: PKG=xr.wargus1 $0 import-data …" >&2; exit 1
		fi
		if adb shell "[ -e '$DEST/extracted' ]" && [[ "${3:-}" != "--replace" ]]; then
			echo "Auf dem Gerät liegen schon Daten ($DEST). Mit --replace überschreiben." >&2; exit 1
		fi
		# Nur Laufzeitdaten: keine Originalarchive, keine Windows-Binärdateien. Tausende kleine
		# Dateien → als ein tar übertragen und auf dem Gerät entpacken.
		TAR="$ROOT/.build/quest-data.tar"
		mkdir -p "$ROOT/.build"
		tar -C "$SRC" -cf "$TAR" --exclude='*.mpq' --exclude='*.MPQ' --exclude='*.war' --exclude='*.WAR' \
			--exclude='*.exe' --exclude='*.dll' --exclude='./support' .
		adb push "$(to_native "$TAR")" /data/local/tmp/xr-stratagus-data.tar
		adb shell "rm -rf '$DEST' && mkdir -p '$DEST' && tar -xf /data/local/tmp/xr-stratagus-data.tar -C '$DEST'; \
			rc=\$?; rm -f /data/local/tmp/xr-stratagus-data.tar; exit \$rc"
		rm -f "$TAR"
		echo "Importiert nach $DEST"
		;;
	start)
		adb shell am start -n "$ACTIVITY"
		;;
	stop)
		adb shell am force-stop "$PKG"
		;;
	logs)
		# Sitzungslogs der XR-Schicht (files/logs/xr-*.log) nach .build/quest-logs holen
		OUT="$ROOT/.build/quest-logs"
		mkdir -p "$OUT"
		adb pull "/sdcard/Android/data/$PKG/files/logs/." "$(to_native "$OUT")" >/dev/null
		ls -t "$OUT" | head -5 | sed "s|^|$OUT/|"
		;;
	log)
		adb logcat -s Stratagus:V SDL:V SDL/APP:V DEBUG:V AndroidRuntime:E
		;;
	*)
		sed -n '2,9p' "$0"; exit 1
		;;
esac
