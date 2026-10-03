// OpenXR-Host für Stratagus/Wargus auf der Quest.
//
// Aufbau: Die Engine rendert unverändert per SDL_Renderer ins Fenster. Vor jedem
// SDL_RenderPresent (Linker-Wrap, xr_present_hook.cpp) kopiert der Spielthread den fertigen
// Frame in eine geteilte Textur. Ein eigener XR-Thread mit eigenem, geteiltem GL-Kontext
// betreibt die OpenXR-Frameschleife mit voller Bildrate – unabhängig davon, ob die Engine
// gerade lädt – und zeigt Spielbild, Ladeanzeige, VR-Menü, VR-Tastatur und Strahl.
// Controller-Eingaben gehen als SDL-Ereignisse an die Engine.
// Übernommen aus XR-CorsixTH (quest/native/xr), angepasst an SDL2 und Wargus.
#pragma once

struct SDL_Renderer;
struct SDL_Window;

namespace xrs {

// Aus dem Present-Hook im Spielthread (SDL-GL-Kontext ist aktuell).
// Liefert true, wenn der Frame an die XR-Darstellung ging und das normale Present entfallen soll.
bool presentFrame(SDL_Renderer* renderer, SDL_Window* window);

// Signale aus der Engine (threadsicher).
void onEngineLine(const char* line);  // jede Ausgabezeile der Engine (Fehlererkennung)
void onEngineCursor();                // Engine setzt einen Mauszeiger: Oberfläche steht
void onMovieStart();                  // Film beginnt (YV12-Textur angelegt)
void onMovieEnd();                    // Film endet (YV12-Textur freigegeben)

// P3 „Spiel-Mauszeiger: Aus“: soll die Engine ihren Mauszeiger zeichnen? (threadsicher; abgefragt vom
// engine-spezifischen Hook native/src/cursor_hook.cpp – die XR-Schicht selbst bleibt engine-neutral).
bool gameCursorVisible();

}  // namespace xrs
