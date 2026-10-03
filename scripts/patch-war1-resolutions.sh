#!/usr/bin/env bash
# War1gus in VR: nur Auflösungen anbieten, bei denen die Oberfläche lesbar bleibt. War1gus zeichnet die
# Oberfläche in Originalpixeln (320×200-Grafik); das VR-Spielbild ist fest 1280×800 (Nearest-Skalierung).
# 1280×800 macht die Oberfläche winzig (Headset 03.10.2026), 320×200 zu grob, Breitbild passt nicht ins
# 16:10-Fenster. Angeboten: 640×400 (2×, scharf), 800×500 und 960×600 (mehr Übersicht). Andere gespeicherte
# Werte werden beim Start auf 640×400 gesetzt (nur auf der Quest: XRS_USER_DIR gesetzt).
#
#   scripts/patch-war1-resolutions.sh <war1gus-datenordner>     # idempotent
set -euo pipefail
D="${1:?Datenordner angeben}"
OPT="$D/scripts/menus/options.lua"
CFG="$D/scripts/stratagus.lua"
[[ -f "$OPT" && -f "$CFG" ]] || { echo "$D ist kein War1gus-Datensatz" >&2; exit 1; }
if ! grep -q 'VR large' "$OPT"; then
	python3 - "$OPT" <<'PY' || python - "$OPT" <<'PY2'
import sys,re
p=sys.argv[1]; s=open(p,encoding='utf-8').read()
new='local resolutions = {\n     "640x400 (VR large)",\n     "800x500 (VR medium)",\n     "960x600 (VR small)"\n  }'
s2,n=re.subn(r'local resolutions = \{.*?\n  \}', new, s, count=1, flags=re.S)
assert n==1, "Aufloesungsliste nicht gefunden"
open(p,'w',encoding='utf-8',newline='\n').write(s2)
PY
import sys,re
p=sys.argv[1]; s=open(p,encoding='utf-8').read()
new='local resolutions = {\n     "640x400 (VR large)",\n     "800x500 (VR medium)",\n     "960x600 (VR small)"\n  }'
s2,n=re.subn(r'local resolutions = \{.*?\n  \}', new, s, count=1, flags=re.S)
assert n==1, "Aufloesungsliste nicht gefunden"
open(p,'w',encoding='utf-8',newline='\n').write(s2)
PY2
	echo "VR-Auflösungen im Optionsmenü: $OPT"
fi
if ! grep -q 'xr: VR-Aufl' "$CFG"; then
	grep -q '^SetVideoResolution(preferences.VideoWidth, preferences.VideoHeight)' "$CFG" || {
		echo "$CFG: SetVideoResolution nicht gefunden" >&2; exit 1; }
	TMP="$CFG.xr"
	awk '/^SetVideoResolution\(preferences.VideoWidth, preferences.VideoHeight\)/ {
		print "-- xr: VR-Auflösungen (scripts/patch-war1-resolutions.sh): Oberfläche bleibt lesbar"
		print "if os.getenv(\"XRS_USER_DIR\") then"
		print "  local w, h = preferences.VideoWidth, preferences.VideoHeight"
		print "  if not ((w == 640 and h == 400) or (w == 800 and h == 500) or (w == 960 and h == 600)) then"
		print "    preferences.VideoWidth = 640"
		print "    preferences.VideoHeight = 400"
		print "  end"
		print "end"
	} { print }' "$CFG" > "$TMP" && mv "$TMP" "$CFG"
	echo "VR-Auflösung beim Start geprüft: $CFG"
fi
