// „Spiel-Mauszeiger: Aus“ (VR-Menü, Pflichtumfang P3) ohne Eingriff in Stratagus.
//
// Linker-Wraps auf DrawCursor()/HideCursor() (src/video/cursor.cpp). DrawCursor zeichnet außer dem
// Mauszeiger auch Auswahlrahmen und Bauplatz-Umriss – die bleiben: Für den Aufruf wird nur der
// aktuelle Spielzeiger (GameCursor) kurz auf nullptr gesetzt, dann zeichnet die Engine alles außer dem
// Zeiger. HideCursor stellt im Menü den Hintergrund unter dem Software-Zeiger wieder her und darf das
// nur, wenn der letzte DrawCursor ihn wirklich gezeichnet (und den Hintergrund gesichert) hat.
// Alles im Spielthread. Mangled Namen: siehe target_link_options in native/CMakeLists.txt.

#include "../xr/xr_app.h"

class CCursor;
extern CCursor *GameCursor;  // Stratagus, src/video/cursor.cpp

namespace
{
bool g_cursorDrawn = false;  // hat der letzte DrawCursor den Zeiger gezeichnet?
}

extern "C" {

void __real__Z10DrawCursorv();  // DrawCursor()
void __real__Z10HideCursorv();  // HideCursor()

void __wrap__Z10DrawCursorv()
{
	if (xrs::gameCursorVisible()) {
		__real__Z10DrawCursorv();
		g_cursorDrawn = true;
		return;
	}
	CCursor *saved = GameCursor;
	GameCursor = nullptr;
	__real__Z10DrawCursorv();
	GameCursor = saved;
	g_cursorDrawn = false;
}

void __wrap__Z10HideCursorv()
{
	if (g_cursorDrawn) {
		__real__Z10HideCursorv();
	}
}

}  // extern "C"
