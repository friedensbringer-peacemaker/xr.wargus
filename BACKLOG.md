# Backlog xr.wargus2

Prio: P0 blockiert Spielen, P1 wichtig, P2 später. Status: Offen → Implementiert (Version) →
Quest-Abnahme offen → Im Headset bestätigt.

## Als Nächstes

| ID | Prio | Status | Aufgabe + Abnahmekriterium |
|---|---|---|---|
| TEST-001 | P0 | Offen | Headset-Testblock T1 nach `TEST.md` (19 Punkte) für 0.2.3-xr durchführen und protokollieren |
| VR-001 | P0 | Quest-Abnahme offen (0.2.0) | OpenXR-Fenster: Spielbild 1280×800 im Quad, zentriert vor der Blickrichtung, Ladeanzeige |
| INPUT-001 | P0 | Quest-Abnahme offen (0.2.0) | Strahl = Maus, Trigger = Links (Rahmen), Griff = Rechts, Stick = Scrollen, A/X = Umschalt/Strg |
| UX-001 | P1 | Quest-Abnahme offen (0.2.0) | VR-Menü im Wargus-Stil (3 Reiter, Mensch/Ork) – Aussehen im Headset bestätigen |
| FLOW-001 | P1 | Quest-Abnahme offen (0.2.0) | Filme überspringen (B, Menü) und „Filme auslassen“ |
| AUDIO-001 | P1 | Quest-Abnahme offen (0.1) | MIDI-Musik über TiMidity + Freepats, Film-Ton über libvorbisfile |
| STAB-001 | P1 | Quest-Abnahme offen (0.2.0) | Beenden sofort (SDL_APP_TERMINATING), Brille ab/auf ohne Hänger |

## Danach

| ID | Prio | Status | Aufgabe + Abnahmekriterium |
|---|---|---|---|
| VR-004 | P1 | Offen | Höhere Renderauflösung (1600×1000, 1920×1200) wählbar mit 15-s-Bestätigung und Rückfall; Surface-Größe und Wargus-Auflösung gemeinsam |
| INPUT-002 | P2 | Offen | Kontrollgruppen per Langdruck (kurz = wählen, 3 s = speichern mit Countdown) statt Menü-Modus |
| UX-002 | P2 | Offen | Ladeanzeige ebenfalls in Wargus-Schrift |
| I18N-001 | P2 | Offen | Umlaute aus der VR-Tastatur (Stratagus nimmt nur ASCII) – ggf. Umschreibung ä→ae |
| PERF-001 | P2 | Offen | Leistung messen (Stats-Zeile im Log), lange Sitzung ≥ 30 min |

## Später einzeln

| ID | Prio | Status | Aufgabe + Abnahmekriterium |
|---|---|---|---|
| REL-001 | P2 | Offen | Preview-Release: THIRD-PARTY.md, Release-Signatur, Spieler-README (GPL: Quellen öffentlich) |
| VR-010 | P2 | Recherche | Tischansicht/Diorama wie XR-Settlers2.5 |
| NAME-001 | P2 | Implementiert (0.3.0-xr) – Quest-Abnahme offen | War1gus als eigene App `xr.wargus1` (Flavor, `scripts/prepare-data-war1.sh`); Abnahme: startet bis ins Hauptmenü, Gefecht spielbar, Musik/Filme, VR-Menü – Testplan `TEST.md` Abschnitt War1gus |
