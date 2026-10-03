# Status xr.wargus2

Zuletzt aktualisiert: 2026-09-30 · Gerät: Meta Quest 3

**Geprüft** heißt: im Headset beobachtet (Screenshot, Video oder Rückmeldung des Nutzers) – nicht nur
gebaut oder per Log bestätigt.

## Rückroll-Punkte (Git-Tags)

| Tag | Commit | Inhalt | Zustand |
|---|---|---|---|
| `headset-ok-2026-09-27` | `1ebe116` | flache Panel-App (S1): Menü sichtbar und bedienbar | im Headset geprüft (Screenshot) |

## Apps

| Label/App-ID | Inhalt | Spieldaten-Pfad auf dem Gerät |
|---|---|---|
| `xr.wargus2` | Stratagus (master) + Wargus-Skripte, OpenXR | `/sdcard/Android/data/xr.wargus2/files/data.Wargus` |
| `xr.wargus1` | dieselbe Engine/XR-Schicht + War1gus-Skripte (ab 0.3.0-xr) | `/sdcard/Android/data/xr.wargus1/files/data.War1gus` |

## Geprüft im Headset

- 0.1.0-s1: Start bis ins Haupt- und Optionsmenü als 2D-Panel, Controller-Laser bedient das Menü
  (Screenshot 2026-09-27).

## Gebaut, noch nicht geprüft (0.2.3-xr)

Automatische Prüfungen bestanden (`docs/VALIDATION.md`); im Headset zu prüfen nach `TEST.md`:
- OpenXR-Fenster, Ladeanzeige, Strahl/Zielpunkt, Passthrough (VR-001)
- Steuerung Trigger/Griff/Stick/A/X/B/Y (INPUT-001)
- VR-Menü im Wargus-Stil, VR-Tastatur (UX-001)
- Filme überspringen/auslassen (FLOW-001), Musik und Film-Ton (AUDIO-001)
- Sofortiges Beenden, Brille ab/auf (STAB-001)

## Gebaut, noch nicht geprüft (0.3.0-xr, xr.wargus1)

Zweite App aus demselben Build (NAME-001). APK `android/build/outputs/apk/war1gus/debug/XR-War1gus-debug.apk`,
Datensatz `original-data/war1gus-quest` (93 MB). Weder installiert noch gestartet – auch nicht am PC.
Im Headset zu prüfen nach `TEST.md` (Abschnitt War1gus).

## Headset-Rückmeldung 2026-09-30

- „Spiel ist gar nicht gestartet“ (0.2.1-xr) → Absturz im ersten Frame durch überdeckte GL-Symbole,
  behoben in 0.2.2-xr (siehe CHANGELOG).
- „Menü links geladen, aber das Spiel hing fest“ (0.2.2-xr, 01.10. 23:50) → vermutlich Pause durch
  Fensterfokus-Verlust, behoben in 0.2.3-xr; Log wird mit 0.2.3 lesbar, erneuter Start steht aus.

## Bekannt offen

- VR-004 Renderauflösung fest 1280×800
- I18N-001 Umlaute aus der VR-Tastatur kommen nicht an
