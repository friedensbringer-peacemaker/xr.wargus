#!/usr/bin/env bash
# Zufallskarte ins Benutzerverzeichnis der App schreiben: Stratagus speichert sie sonst als
# maps/randommap.smp im Datenordner, den die App auf der Quest nicht beschreiben darf (Datenordner per
# adb importiert) → Karte fehlt, Absturz beim Spielstart (Log 03.10.2026 07:14, War1gus). Absolute Pfade
# nimmt Stratagus unverändert an (EditorSaveMap, LibraryFileName); XRS_USER_DIR setzt android_main.cpp.
# Am PC (ohne XRS_USER_DIR) bleibt alles beim Alten.
#
#   scripts/patch-random-map.sh <datenordner>     # idempotent, Wargus und War1gus
set -euo pipefail
D="${1:?Datenordner angeben}"
NEW='mapname = (os.getenv("XRS_USER_DIR") and (os.getenv("XRS_USER_DIR") .. "/randommap.smp")) or "maps/randommap.smp" -- xr'
n=0
for f in "$D/scripts/guichan.lua" "$D/scripts/lists/maps/Random Skirmish"; do
	[[ -f "$f" ]] || continue
	if grep -q 'mapname = "maps/randommap.smp"$' "$f"; then
		sed -i "s|mapname = \"maps/randommap.smp\"\$|$NEW|" "$f"
		echo "Zufallskarte ins Benutzerverzeichnis: $f"
	fi
	grep -q 'XRS_USER_DIR' "$f" && n=$((n + 1))
done
[[ $n -gt 0 ]] || { echo "$D: keine Zufallskarten-Stelle gefunden" >&2; exit 1; }
