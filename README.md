# XR-Stratagus

VR-Port-Vorhaben für die Stratagus-Engine auf Basis der XRShell-Plattform (`../XR-Ports-Platform`).
Zwei Apps aus einem Build: `xr.wargus2` (Wargus = Warcraft-II-Spielpaket) und `xr.wargus1`
(War1gus = Warcraft-I-Spielpaket, ab 0.3.0-xr). Spiele-Assets nur lokal unter
`original-data/` (gitignored, eigene GOG-Kopie des Spielers) — nie committen, nie weitergeben.

## Inhalt (Stand 2026-09-27)

| Pfad | Quelle | Stand |
|------|--------|-------|
| `upstream/stratagus` | https://github.com/Wargus/stratagus (GPL-2.0) | master `3d87c93f7` (2026-06-22), letzter Tag v3.3.2 |
| `upstream/wargus` | https://github.com/Wargus/wargus (GPL-2.0) | master `cde1a071` (2026-06-29), letzter Tag v3.3.2 |
| `upstream/war1gus` | https://github.com/Wargus/war1gus (GPL-2.0) | master `8ec75764c` (2026-06-22), passend zum Engine-master |
| `upstream/*/third-party` | https://github.com/Wargus/dependencies (Submodul) | SDL2, SDL2_image/mixer, Lua 5.1.5, guichan, theora, bzip2 … |
| `release/Wargus-portable-3.3.2.exe` | Wargus-Release v3.3.2 (2022-08-10) | Windows, portable, enthält keine Blizzard-Daten (lokal, gitignored) |
| `release/compiled-binaries.zip` | Stratagus-Release v3.3.2 | Windows-Engine + DLLs (lokal, gitignored) |
| `original-data/wargus-desktop/` | Release 3.3.2 + `wartool -v` aus GOG-Daten | lokale Desktop-Referenz, Start `stratagus.exe -d .` |

Klonen: `git clone --recursive …` bzw. `git submodule update --init --recursive`.
Nächste Schritte: [docs/ROADMAP.md](docs/ROADMAP.md).

Das letzte offizielle Release ist von 2022; die Entwicklung läuft auf `master` weiter —
für den Port ist `master` maßgeblich.

## Spieldaten (vom Spieler)

War1gus: `war1tool` (in `upstream/war1gus`) liest `DATA/DATA.WAR` der eigenen DOS-/GOG-Installation;
`scripts/prepare-data-war1.sh original-data/warcraft1` → `original-data/war1gus-quest`.

`wartool` (in `upstream/wargus`) akzeptiert: DOS-CD (`REZDAT.WAR`), Battle.net Edition
(`INSTALL.MPQ`/`INSTALL.EXE`) und den GOG-Installer. Musik/Video-Konvertierung braucht ffmpeg.

## Referenzen

- Android-Fork: https://github.com/drodin/Stratagus
- PS-Vita-Port: https://github.com/Northfear/stratagus-vita

## Lizenz

GPL-2.0 wie Stratagus und Wargus (siehe `COPYING`); eingebundene Bibliotheken behalten ihre
eigenen Lizenzen. `<XR-Ordner>` meint in der Doku den gemeinsamen Ordner aller XR-Projekte.

**Keine fertigen APKs (REL-001):** Stratagus steht unter GPL-2.0-only. Für eine Quest-APK wird
zusätzlich der Khronos-OpenXR-Loader gebraucht, der unter Apache-2.0 steht – laut FSF ist das mit
GPL-2.0-only nicht verträglich. Deshalb veröffentlicht dieses Repo nur den Quellcode, der für sich
unbedenklich ist, und keine APKs. Wer die App nutzen möchte, baut sie selbst und verwendet dabei
die eigene Kopie der Spieldaten.
