package xr.stratagus;

import android.content.Context;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

/**
 * Einstieg der App. SDL2 und die Engine (libmain.so, SDL_main) werden geladen;
 * Datenpfade, Engine-Argumente und die OpenXR-Darstellung übernimmt der native Teil
 * (native/src/android_main.cpp, native/xr). Dieselbe Klasse dient beiden Apps (Gradle-Flavors):
 * xr.wargus2 (Wargus) und xr.wargus1 (War1gus); das Spielpaket wählt der native Teil nach der
 * App-ID (native/xr/xr_game.h).
 */
public class StratagusActivity extends SDLActivity {
    /** Feste Renderfläche: das Spielbild wird 1:1 in das VR-Fenster (Quad-Layer) kopiert. */
    private static final int RENDER_WIDTH = 1280;
    private static final int RENDER_HEIGHT = 800;

    @Override
    protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL2", "main" };
    }

    /** App-ID an SDL_main: wählt das Spielpaket (Datenordner, Einstellungen, Menüstil). */
    @Override
    protected String[] getArguments() {
        return new String[] { "--xr-app=" + getPackageName() };
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        SDLSurface surface = super.createSDLSurface(context);
        surface.getHolder().setFixedSize(RENDER_WIDTH, RENDER_HEIGHT);
        return surface;
    }
}
