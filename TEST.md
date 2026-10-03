# Headset-Testplan xr.wargus2 0.2.3-xr (Testblock T1)

Ziel: erster Headset-Test der VR-Fassung (OpenXR-Fenster, Steuerung, VR-Menü). Vorher automatisch
geprüft: siehe `STATUS.md` („Gebaut, nicht geprüft“) und `docs/VALIDATION.md`.

**Vorbereitung (PC):** Quest per USB, Dauer-Logger läuft (`<XR-Ordner>/_tools/quest`). Installierte
Version prüfen: `adb shell dumpsys package xr.wargus2 | grep versionName` → `0.2.3-xr`.
Spieldaten liegen unter `Android/data/xr.wargus2/files/data.Wargus` (inkl. `timidity/`).

**Start:** Bibliothek → „Unbekannte Quellen“ → `xr.wargus2`. Bei Auffälligkeiten Screenshot (Meta-Taste +
Trigger) – er landet unter `Oculus/Screenshots/xr.wargus2`.

## Checkliste (in dieser Reihenfolge)

| # | Schritt | Erwartung | ok? |
|---|---|---|---|
| 1 | App starten | Ladeanzeige „xr.wargus2 – Das Spiel wird geladen“ auf Stein, danach Intro-Film im Fenster vor dir | |
| 2 | Film: B drücken | Logo-/Intro-Film endet sofort; Film hat **Ton** | |
| 3 | Hauptmenü ansehen | Bild füllt das Fenster (1280×800, keine schwarzen Balken), scharf genug zum Lesen; **Menümusik** läuft | |
| 4 | Laser auf Menüpunkte | goldener Strahl + Ring; Mauszeiger des Spiels folgt genau dem Ring (auch an den Rändern) | |
| 5 | Trigger auf „Einzelspieler“ o. ä. | Klick kommt an (Haptik kurz), Menü wechselt | |
| 6 | Linke Menütaste | VR-Menü im Warcraft-Stil über der linken Hand; Reiter umschaltbar; B schließt | |
| 7 | VR-Menü → Fenster & Bild | „Fenster näher/weiter/größer/kleiner“ wirkt; „Menüstil: Ork“ ändert Farben; Passthrough an/aus | |
| 8 | Linker Griff halten + linker Stick | Fenster breiter/schmaler (links/rechts), näher/weiter (vor/zurück) | |
| 9 | Kampagne/Gefecht starten | Karte lädt; Kampagnenfilm überspringbar (B) | |
| 10 | Rechter Stick | Karte scrollt in alle vier Richtungen; kein Scrollen am Bildrand per Laser | |
| 11 | Trigger ziehen | Auswahlrahmen, Einheiten ausgewählt | |
| 12 | Rechter Griff auf Boden/Feind | Einheiten laufen bzw. greifen an | |
| 13 | A halten + Trigger auf Einheit | Einheit zur Auswahl hinzu | |
| 14 | VR-Menü → Gruppen: „speichern“, Gruppe 1; dann „wählen“, Gruppe 1 | Gruppe gespeichert und wieder gewählt | |
| 15 | Y kurz / Y lang | Pause an/aus / Spielmenü | |
| 16 | VR-Menü → Speichern (F11), Tastatur → „Name vorschlagen“, Übernehmen | Spielstand gespeichert | |
| 17 | Brille 30 s absetzen, wieder aufsetzen | Spiel pausiert/läuft weiter, Fenster wieder vor dir | |
| 18 | Meta-Taste → Beenden | App schließt sofort (keine 10 s) | |
| 19 | App erneut starten | VR-Einstellungen (Fenstermaße, Stil) wiederhergestellt; mit „Filme auslassen: an“ kein Intro | |

## Erwartete Logzeilen (`scripts/quest.sh log` bzw. Sitzungslog `scripts/quest.sh logs`)

- `xr: SDL-Kontext: EGL-Client …, OpenGL ES 3.2 …`, `xr: Runtime Oculus …`, `xr: Swapchain Spiel: 1280x800`
- `theme: Menüstil Mensch (Wargus): Panel Spielgrafik, Knöpfe Spielgrafik, Schrift Spiel` (ab 0.3.0-xr)
- `xr: Session … -> FOCUSED`, `window: zentriert: Breite 2.00 m, Abstand 1.80 m`
- `movie: Film 1 beginnt`, `movie: Film übersprungen (…)`
- `stats: Spiel … fps …, XR 72.0 Hz …` alle 10 s
- keine `E xr:`-Zeilen, kein `FATAL`, kein `stack traceback`

## War1gus (`xr.wargus1`, 0.3.0-xr) – Kurzblock

Vorbereitung: `PKG=xr.wargus1 scripts/quest.sh install`, `PKG=xr.wargus1 scripts/quest.sh import-data
original-data/war1gus-quest`; `dumpsys package xr.wargus1 | grep versionName` → `0.3.0-xr`.

| # | Schritt | Erwartung | ok? |
|---|---|---|---|
| W1 | `xr.wargus1` starten | Ladeanzeige „xr.wargus1“, Logo, dann Intro-Filme (B überspringt), mit Ton | |
| W2 | Hauptmenü | füllt das Fenster ohne Balken (640×400), lesbar; Menümusik (MIDI) | |
| W3 | Laser am Bildrand / außerhalb | Karte scrollt **nicht** von selbst | |
| W4 | Linke Menütaste | VR-Menü mit War1gus-Panel und -Schrift, Knöpfe gezeichnet; „Menüstil: Ork“ wechselt Panel | |
| W5 | Gefecht/Kampagne Mensch 1 | Karte lädt, Rahmen wählt, Griff befiehlt, Stick scrollt | |
| W6 | Spielstand speichern (F11) + Tastatur | Spielstand gespeichert | |
| W7 | Beenden über VR-Menü, neu starten | sofort beendet; Einstellungen erhalten; xr.wargus2 unverändert startbar | |

Logzeilen: `start: xr.wargus1 (War1gus), Daten: …/data.War1gus`, `theme: Menüstil Mensch (War1gus): Panel
Spielgrafik, Knöpfe Ersatz, Schrift Spiel`, `VR-Voreinstellungen xr-defaults-v1 geschrieben` (nur beim ersten Start).

## Bekannte Grenzen dieses Builds

- Umlaute kommen aus der VR-Tastatur nicht im Spiel an (Stratagus nimmt nur ASCII).
- Renderauflösung fest 1280×800 (höher = mehr Karte folgt, BACKLOG VR-004).
