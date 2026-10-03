# App-Icons – Herkunft

| App | Dateien | Herkunft |
|---|---|---|
| `xr.wargus2` (Flavor `stratagus`) | `android/src/stratagus/res/mipmap-{m,h,xh,xxh,xxxh}dpi/ic_launcher.png` (48/72/96/144/192 px) | Eigene Grafik, prozedural erzeugt mit `tools/launcher-icon/make_icon.py` (wie unten). |
| (Rückfall, ungenutzt) | `android/src/main/res/mipmap-*/ic_launcher.png` | Unverändertes Standard-Icon der SDL2-Android-Projektvorlage (zlib-Lizenz, identisch mit `upstream/stratagus/third-party/SDL/android-project/…`, übernommen in `d7521ac`). Wird von beiden Flavors überdeckt. |
| `xr.wargus1` (Flavor `war1gus`) | `android/src/war1gus/res/mipmap-{m,h,xh,xxh,xxxh}dpi/ic_launcher.png` (48/72/96/144/192 px) | Eigene Grafik, prozedural erzeugt mit `tools/launcher-icon/make_icon.py` (nur Python-Standardbibliothek, keine Fremdgrafik, keine Schrift, keine Spieldaten). Gehört zum Projekt (Lizenz wie das Repo). |

Neu erzeugen: `python tools/launcher-icon/make_icon.py --preview` (schreibt je Flavor die fünf PNGs und
`docs/icons/launcher-preview.png`). Die Flavor-Ressourcen überdecken `main/res`, das Manifest
(`android:icon="@mipmap/ic_launcher"`) bleibt gemeinsam.

## Gestaltung (Familie: Schild + römische Ziffer)

- **Kachel** am SDL-Standard-Icon ausgemessen: abgerundetes Quadrat 6,5 %…93,7 %, Radius 7,5 %,
  Rand `#B8B8B8`, Fläche weiß → `#D3D3D3` nach unten, weicher Schatten. Gleich für beide Apps.
- **Motiv `xr.wargus1`:** heraldischer Dreieckschild (Breite 56 %), Feld weinrot `#8E2A1F` → `#521410`, Goldrand
  `#D9A441`, darin römische Ziffer **I** mit Serifen in Gold `#F6D36B` → `#C98A2A`, dunkle Kontur
  `#240C08` für Lesbarkeit bei 48 px. Erdige Rot-/Goldtöne = älteres, rauheres Spiel.
- Kein Markenbezug: kein Schriftzug, kein Logo, keine Figur; nur Schild + Ziffer.
- **Motiv `xr.wargus2`** (Variante (a), vom Nutzer gewählt 2026-10-03): gleicher Schild in Blau
  `#2C4F80` → `#162A4A` mit Silberrand `#C8CFD8` und Ziffer **II** in demselben Gold.

Vorschau `docs/icons/launcher-preview.png` (auf Quest-ähnlich dunklem Grund, von links):
`xr.wargus2` in 192/96/48 px, dann `xr.wargus1` in 192/96/48 px.
