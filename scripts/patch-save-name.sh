#!/usr/bin/env bash
# Speichern-Dialog: statt immer „game.sav“ (überschreibt den letzten Stand) einen Namen aus Datum,
# Uhrzeit und Karte vorschlagen, z. B. „2026-10-03_06-55-Kartenname.sav“. Wird auf den aufbereiteten
# Datensatz angewendet (prepare-data.sh / prepare-data-war1.sh), Upstream-Skripte bleiben unverändert.
#
#   scripts/patch-save-name.sh <datenordner>     # idempotent
set -euo pipefail
F="${1:?Datenordner angeben}/scripts/menus/save.lua"
[[ -f "$F" ]] || { echo "$F fehlt" >&2; exit 1; }
grep -q 'XrSaveName' "$F" && exit 0
grep -q 'addTextInputField("game.sav"' "$F" || { echo "$F: Vorschlag „game.sav“ nicht gefunden" >&2; exit 1; }
TMP="$F.xr"
cat > "$TMP" <<'LUA'
-- xr: Speichername vorschlagen (Datum_Uhrzeit-Karte) statt immer "game.sav" (scripts/patch-save-name.sh)
local function XrSaveName()
  local name = os.date("%Y-%m-%d_%H-%M")
  local ok, desc = pcall(function() return Map.Info.Description end)
  if ok and type(desc) == "string" and desc ~= "" then
    desc = string.gsub(desc, "[^%w]+", "_")
    name = name .. "-" .. string.sub(desc, 1, 20)
  end
  return name .. ".sav"
end

LUA
sed 's/addTextInputField("game\.sav"/addTextInputField(XrSaveName()/' "$F" >> "$TMP"
mv "$TMP" "$F"
echo "Speichername-Vorschlag eingebaut: $F"
