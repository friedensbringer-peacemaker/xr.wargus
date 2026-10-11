# Bedienung in VR (xr.wargus2 0.2.0-xr)

Das Spiel erscheint als Fenster (2 m breit, 1,8 m entfernt) vor der Blickrichtung. Belegung
nach den Regeln aus den anderen XR-Ports (Skill `xr-quest-port`), angepasst an Warcraft II.

## Controller

| Eingabe | Funktion |
|---|---|
| Strahl der Zeigerhand (Standard rechts) | Mauszeiger |
| Trigger (Zeigerhand) | Linksklick: auswählen; halten + ziehen = Auswahlrahmen |
| Griff (Zeigerhand) kurz | Rechtsklick beim Loslassen: Befehl (bewegen, angreifen, ernten …) |
| Griff (Zeigerhand) halten + Hand bewegen | **Karte greifen und ziehen**, in alle Richtungen (Tempo: VR-Menü → Steuerung → „Karte ziehen“) |
| Stick (Zeigerhand) | Karte stufenlos scrollen, auch schräg (Tempo: VR-Menü → Steuerung → „Scrolltempo“) |
| Stick drücken (Zeigerhand) | Auswahl zentrieren |
| A halten | Umschalt: Einheiten zur Auswahl hinzufügen/entfernen |
| X halten | Strg (z. B. Strg + Klick) |
| B | Escape: Befehl abbrechen, Dialog schließen, **Film überspringen**; schließt auch Menü/Tastatur |
| Y kurz / lang | Pause / Spielmenü (F10) |
| Linke Menütaste | VR-Menü |
| Griff der anderen Hand halten + Hand heranziehen / wegschieben | **Zoom**: Fenster bzw. Tisch größer / kleiner |
| Griff der anderen Hand + deren Stick | Fenster breiter/schmaler (links/rechts), näher/weiter (vor/zurück) |
| … dazu Stick drücken | Fenster zurücksetzen und zentrieren |

Ein Trigger- oder Griffdruck neben dem Fenster wird ignoriert; bei Fokus- oder Trackingverlust
werden gehaltene Tasten gelöst.

## VR-Menü (linke Menütaste)

Gestaltet wie die Wargus-Menüs; Grafiken und Schrift kommen aus den eigenen Spieldaten auf dem
Gerät (`data.Wargus/graphics/ui`). Das Menü hängt über der linken Hand.

Bedienung: Strahl + Trigger **oder** Stick (goldener Fokusrahmen) + A; B oder Menütaste schließen.
Regler (Größe, Abstand, Bild, Tempo) wiederholen bei gehaltenem Trigger/A.

**Schnellmenü** (öffnet immer zuerst): Weiterspielen, Fenster zentrieren, Passthrough, Spielmenü (F10),
Tastatur, Einstellungen, **Beenden** (rot: erster Klick „Sicher? Nochmal klicken“, zweiter Klick
innerhalb 3 s speichert und beendet), Schließen.

**Gruppen** (unten im Schnellmenü): Modus **Wählen / Speichern / Ergänzen** (aktiver Modus hell und gold
unterstrichen), darunter die Knöpfe **1–9**. Ein Klick auf eine Zahl wählt die Gruppe (Wählen), legt die
aktuelle Auswahl als Gruppe ab (Speichern = Strg+Zahl) bzw. fügt sie hinzu (Ergänzen = Strg+Umschalt+Zahl)
und schließt das Menü. Danach steht der Modus wieder auf „Wählen“. Zweimal dieselbe Zahl wählen
zentriert die Ansicht auf die Gruppe (Stratagus).

**Warcraft I – Auflösung** (Spieloptionen): nur „640x400 (VR large)“ (empfohlen, scharf), „800x500 (VR
medium)“ und „960x600 (VR small)“ (mehr Kartensicht, leicht ungleichmäßige Pixel). Höhere Auflösungen
machen die Oberfläche in VR unlesbar klein und werden beim Start auf 640×400 zurückgesetzt.

**Speichern-Dialog:** schlägt Datum, Uhrzeit und Karte als Namen vor (z. B.
`2026-10-03_06-55-Kartenname.sav`) statt immer `game.sav`.

**Einstellungen** (jede Seite endet mit „Zurück“ / „Schließen“):
- **Spiel:** Pause, Hilfe (F1), Speichern (F11), Laden (F12), Tempo +/−, Untätiger Arbeiter,
  Letzte Meldung, Auswahl zentrieren, **Film überspringen** (nur während eines Films),
  **Filme auslassen** (Intro und Kampagnenfilme automatisch überspringen, bleibt gespeichert),
  Eingaben loggen, Diagnose ins Log.
- **Fenster:** zurücksetzen, **Ansicht Leinwand / Tisch** (Tisch wie in Siedler: Spielbrett liegt gekippt
  vor dir), kleiner/größer, näher/weiter; im Tisch zusätzlich **Neigung** (30–75° aus der Senkrechten, 5° je
  Klick) und **Tisch höher/tiefer** (0–90 cm unter Blickhöhe). Größe und Abstand gelten je Ansicht.
- **Steuerung:** Zeigerhand, **Karte ziehen 1x/1,5x/2x/3x** (wie stark die Karte der Hand folgt),
  **Scrolltempo 1–10** (Stick), **Zeiger waagerecht / senkrecht** (‹ = links/oben, › = rechts/unten, je
  0,25 %, bis ±15 %; gehalten wiederholt): verschiebt den Spiel-Mauszeiger gegenüber dem Laserpunkt.
- **Anzeige:** Strahl an/aus, Zielpunkt Ring/Punkt/aus, **Spiel-Mauszeiger an/aus**, Bild
  zurücksetzen, **Bildmodus normal / Scanlines / CRT** (Röhrenlook: Zeilen im 2-Pixel-Raster, bei CRT
  zusätzlich Streifenmaske und dunklere Ränder; ohne Wölbung, damit der Strahl genau trifft; gespeichert),
  Menüstil Mensch/Ork, Helligkeit/Kontrast/Sättigung/Schärfe.

## VR-Tastatur

Über „Tastatur“ im VR-Menü (Stratagus hält die Texteingabe dauerhaft offen, deshalb nicht
automatisch). Tasten mit dem Strahl antippen; „Übernehmen“ = Enter, „Abbrechen“ = Escape, beide
schließen die Tastatur. „Name vorschlagen“ tippt einen Zeitstempel (für Spielstände). Stratagus
nimmt nur ASCII an – Umlaute kommen im Spiel nicht an.

## War1gus (`xr.wargus1`)

Gleiche Steuerung und dasselbe VR-Menü. Unterschiede:
- **Auflösung:** erster Start mit 640×400 (doppelte Originalgröße 320×200, füllt das Fenster scharf);
  „Original pixel ratio“ (Höhe ×1,2 wie am Röhrenmonitor) ist aus, sonst Balken links/rechts. Beides
  im War1gus-Menü Optionen änderbar.
- **Auswahl:** Warcraft I erlaubte höchstens 4 Einheiten. War1gus erlaubt standardmäßig 50 (Rahmen mit
  Trigger wie bei Wargus); das Original-Limit ist in den Optionen wählbar („Max selection: 4 (WC1 default)“).
- **Scrollen** nur per Stick: das Kanten-Scrollen schaltet der Datensatz ab (in War1gus keine Einstellung).
- **VR-Menü:** Steinpanel und Titelschrift aus den War1gus-Daten, Schaltflächen gezeichnet (War1gus-Menüs
  haben keine Knopfgrafiken); „Menüstil Mensch/Ork“ wechselt das Panel.
- Einstellungen: `files/user/wc1/preferences.lua` (Marker `xr-defaults-v1`); Daten unter `files/data.War1gus`.

## Einstellungen und Logs

- VR-Einstellungen: `files/user/xr-settings.txt`, Wargus: `files/user/wc2/preferences.lua`
  (einmalige VR-Voreinstellungen: 1280×800, Kanten-Scrollen aus – Marker `xr-defaults-v2`).
- Sitzungslogs: `files/logs/xr-*.log` (letzte 10), dazu logcat-Tags `xr-stratagus`, `Stratagus`.
- Desktop-Vorschau der VR-Anzeigen ohne Headset: `tools/menu-preview/build.sh`.
