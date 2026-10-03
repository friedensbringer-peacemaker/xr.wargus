# Prüfprotokoll

Ein Abschnitt pro Build. „Bestanden“ gilt nur für die genannte Prüfung; im Headset geprüft ist nur,
was unter „Headset“ steht.

## 0.2.4-xr (versionCode 6) — 2026-10-02, Branch `xr-menu-pflicht`

APK `android/build/outputs/apk/debug/XR-Stratagus-debug.apk`, SHA-256
`7c6b99cc7a5e8c93a9176128411a72b3fd12fadc79bcc5f14f373e6ca9b72491`

| Prüfung | Ergebnis |
|---|---|
| Native Build + Symbolprüfung; Wrap `DrawCursor`/`HideCursor` in `libmain.so` | bestanden, 0 Warnungen |
| Host-Test Eingabe: ConfirmAction, PointerVisibility, HoldRepeat (36/72/90/120 Hz), Stick-Richtung | 2 182 Prüfungen, 0 Fehler |
| Host-Test UI: 5 Seiten × Mensch/Ork × (ohne Film / Film / Beenden scharf): Layout, Treffer, Schnellmenü ≤ 8, Zurück/Schließen, Stick-Fokus (Start, runter, rechts in der Zeile, Rand, nie gesperrt) | 6 952 Prüfungen, 0 Fehler |
| Vorschau-PNGs (Schnellmenü, Beenden scharf, Anzeige-Reiter) | angesehen |
| APK-Gate, `git diff --check` | bestanden |
| Installation (`dumpsys` versionName, 02.10. 20:38) | 0.2.4-xr |
| Headset | offen |

## 0.2.2-xr (versionCode 4) — 2026-09-30

APK `.build/releases/0.2.2-xr/xr.stratagus-0.2.2-xr.apk`, SHA-256
`622fd27f02bbbbd2cf6b56158d1ec7c20dff079197271cb92d64ff331776dc96`

| Prüfung | Ergebnis |
|---|---|
| Headset-Start 0.2.0 (29.09. 22:57) und 0.2.1 (30.09. 21:00, Nutzer) | **Absturz** SIGSEGV im ersten Frame (`GlStateGuard::~GlStateGuard` → `glUseProgram` → `.bss`) |
| Ursache | Stratagus-Variablen `glUseProgram` u. a. überdecken libGLESv3 in libmain.so |
| Native Build + Symbolprüfung (keine B/D-Symbole `gl|egl|xr[A-Z]`) | bestanden, 0 Warnungen |
| Tests am PC (`tests/run.sh host`) | 107 293 / 1 187 / 4 276 Prüfungen, 0 Fehler |
| Tests auf der Quest (`tests/run.sh quest`, ARM64, Gerätebibliotheken und -daten) | 107 293 / 1 187 / 4 276 Prüfungen, 0 Fehler; Menü mit Spielgrafik und -schrift gerendert |
| APK-Gate | bestanden |
| Installation (`dumpsys` versionName) | 0.2.2-xr |
| Headset | offen |

## 0.2.1-xr (versionCode 3) — 2026-09-30

APK: `.build/releases/0.2.1-xr/xr.stratagus-0.2.1-xr.apk`, SHA-256
`cf90d0f0881d1a6ac652f4700f76e9a7520f98d6611ee9028ce20742ea3b6eaa`

| Prüfung | Ergebnis |
|---|---|
| Native Build (NDK 30, `-Wall -Wextra`, eigener Code) | bestanden, 0 Warnungen |
| `git diff --check` | bestanden |
| Host-Test Geometrie (`tests/test_geometry.cpp`): Mathematik, Fenstergrenzen 61×61, Strahltreffer 5 Seitenverhältnisse × 7 Richtungen × 5 Abstände × 3 Breiten × 15 Punkte × 2 Ursprünge + Fehlschüsse | 107 293 Prüfungen, 0 Fehler (15 750 Treffer, 17 325 Fehlschüsse) |
| Host-Test Eingabe (`tests/test_input.cpp`): Hysterese, Re-Arm, Kurz/Lang bei 36/72/90/120 Hz, Deadzone, Glättung | 1 187 Prüfungen, 0 Fehler |
| Host-Test UI (`tests/test_ui.cpp`): Menü 3 Reiter × Mensch/Ork × mit/ohne Film, Tastatur, Ladeanzeige, Einstellungen | 4 276 Prüfungen, 0 Fehler; Spielgrafiken und -schrift geladen |
| Desktop-Vorschau der VR-Anzeigen (`tools/menu-preview`) | angesehen, Wargus-Stil korrekt (Schriftfarben aus fonts.lua) |
| APK-Gate (`scripts/apk-gate.sh`): Libs, nur arm64, keine Spieldaten, Paket, Version, OpenXR-Permission, IMMERSIVE_HMD/VR-Kategorie, headtracking, Runtime-Query, resizeableActivity, Signatur | bestanden |
| Repo: keine Spieldaten/Geheimnisse (`git ls-files`, `git grep`) | bestanden |
| `libmain.so`: NEEDED openxr_loader/EGL/GLESv3/SDL2, Wrap `SDL_RenderPresent`, 16-KB-Ausrichtung | bestanden |
| Tests auf der Quest (ARM64, Gerätebibliotheken, Gerätedaten: `tests/run.sh quest`) | offen – Quest war nicht verbunden |
| Installation (`dumpsys` versionName) | offen |
| Headset (TEST.md, Block T1) | offen |

Nicht automatisch prüfbar: OpenXR-Session/Swapchains auf dem Gerät, GL-Kontext-Teilung mit SDL,
tatsächliche Strahltreffer im Raum, Musik/Ton, Leistung.

## 0.2.0-xr (versionCode 2) — 2026-09-29

Native Build warnungsfrei, APK gebaut und installiert, Daten importiert. Nie gestartet.

## 0.1.0-s1 — 2026-09-27

Im Headset: Haupt- und Optionsmenü sichtbar, Laser-Klicks im Menü (Screenshot). Log: Renderer
`opengles2`, `TIMIDITY MIDI` als Musik-Decoder (nach Nachbesserung).
