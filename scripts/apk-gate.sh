#!/usr/bin/env bash
# APK-Gate für Quest-Ports: bricht ab, wenn Paket, Permissions, Manifest-Attribute, native Libs oder Signatur
# nicht stimmen. Verallgemeinert aus XR-OpenRA build-dotnet-package.sh.
#
#   apk-gate.sh <apk> <paket-id> [erwartete .so ...]
#   z. B. apk-gate.sh Artifacts/xr-foo-quest3.apk xr.foo libmain.so libopenxr_loader.so
#
# Braucht ANDROID_HOME (build-tools mit aapt2/apksigner; sonst ../_tools/android-sdk). Läuft in Git
# Bash, macOS, Linux. Übernommen aus dem Skill xr-port-qa, ergänzt um „keine Spieldaten“.
set -euo pipefail

APK=${1:?APK fehlt}; PKG=${2:?Paket-ID fehlt}; shift 2
SO_LIST=("$@")
[ -f "$APK" ] || { echo "FEHLER: $APK fehlt" >&2; exit 1; }

case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) WIN=1; EXE=.exe ;; *) WIN=0; EXE= ;; esac
SDK=${ANDROID_HOME:-$(cd "$(dirname "$0")/../../_tools/android-sdk" && pwd)}
BT=$(ls -d "$SDK"/build-tools/*/ | sort -V | tail -1)
AAPT2="${BT}aapt2$EXE"
[ -x "$AAPT2" ] || { echo "FEHLER: aapt2 nicht gefunden in $BT" >&2; exit 1; }
apk_arg=$APK; [ "$WIN" = 1 ] && apk_arg=$(cygpath -m "$APK")

fail=0
require() { # require "<Meldung>" '<regex>' "$text"
    if printf '%s\n' "$3" | grep -Eq "$2"; then echo "  ok   $1"; else echo "  FEHLT $1"; fail=1; fi
}

re_esc() { printf '%s' "$1" | sed 's#[][\.*^$+?(){}|/]#\\&#g'; }  # Regex-Sonderzeichen entschärfen
recommend() { # wie require, aber nur Warnung
    if printf '%s\n' "$3" | grep -Eq "$2"; then echo "  ok   $1"; else echo "  WARN $1 (empfohlen)"; fi
}

echo "== Native Libs"
libs=$(unzip -Z1 "$APK")
for so in ${SO_LIST[@]+"${SO_LIST[@]}"}; do require "$so" "^lib/arm64-v8a/$(re_esc "$so")\$" "$libs"; done
require "nur arm64-v8a" '^lib/arm64-v8a/' "$libs"
if printf '%s\n' "$libs" | grep -Eq '^lib/(armeabi|x86)'; then
    echo "  FEHLT nur arm64 (andere ABIs enthalten)"; fail=1
fi

echo "== Keine Spieldaten"
# Nur eigene Ressourcen: keine Wargus-/War1gus-/Originaldaten (Grafiken, Sounds, Videos, Archive, Skripte).
bad=$(printf '%s\n' "$libs" |
    grep -Ei '\.(war|mpq|pud|smk|ogv|mid|wav|ogg|lua|sf2|pat)$|graphics/|data\.War(gus|1gus)|war1data|campaigns/|scripts/' || true)
if [ -n "$bad" ]; then
    echo "  FEHLT keine Spieldaten – gefunden:"; printf '%s\n' "$bad" | head -5 | sed 's/^/       /'; fail=1
else
    echo "  ok   keine Spieldaten in der APK"
fi

echo "== Badging"
badging=$("$AAPT2" dump badging "$apk_arg")
require "Paket $PKG" "^package: name='$(re_esc "$PKG")'" "$badging"
require "versionName gesetzt" "versionName='[^']+'" "$badging"
require "OpenXR-Permission" "uses-permission: name='org.khronos.openxr.permission.OPENXR'" "$badging"
printf '%s\n' "$badging" | grep -E "^package:|targetSdkVersion|launchable-activity" | sed 's/^/       /'
recommend "versionCode > 1 (je weitergegebener APK hochzählen)" "versionCode='([2-9]|[1-9][0-9]+)'" "$badging"

echo "== Manifest"
xml=$("$AAPT2" dump xmltree --file AndroidManifest.xml "$apk_arg")
require "IMMERSIVE_HMD-Kategorie" "org.khronos.openxr.intent.category.IMMERSIVE_HMD" "$xml"
require "Oculus-VR-Kategorie" "com.oculus.intent.category.VR" "$xml"
require "headtracking-Feature" "android.hardware.vr.headtracking" "$xml"
require "OpenXR-Runtime-Query" "org.khronos.openxr.OpenXRRuntimeService" "$xml"
recommend "resizeableActivity=false" "resizeableActivity.*=(false|0x0)" "$xml"

echo "== Signatur"
if [ "$WIN" = 1 ]; then
    out=$(cmd.exe //c "$(cygpath -w "${BT}apksigner.bat")" verify --verbose "$(cygpath -w "$APK")" 2>&1 || true)
else
    out=$("${BT}apksigner" verify --verbose "$APK" 2>&1 || true)
fi
require "apksigner verify" "Verifies" "$out"

echo "== SHA-256: $(sha256sum "$APK" | cut -d' ' -f1)"
if [ "$fail" = 1 ]; then echo "GATE NICHT BESTANDEN – APK nicht weitergeben." >&2; exit 1; fi
echo "GATE BESTANDEN (= gebaut, nicht geprüft)"
