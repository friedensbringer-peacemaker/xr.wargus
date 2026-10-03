// Android-Einstieg für Stratagus (ersetzt upstream src/stratagus/main.cpp).
//
// SDLActivity ruft SDL_main (SDL.h benennt main entsprechend um). Hier werden
//  - stdout/stderr der Engine nach logcat (Tag "Stratagus") und ins Sitzungslog umgeleitet,
//  - Daten-, Benutzer- und Logverzeichnis im app-eigenen externen Speicher festgelegt
//    (Umgebung XRS_DATA_DIR / XRS_USER_DIR / XRS_LOG_DIR für die XR-Schicht),
//  - das Spielpaket nach der App-ID gewählt (xr_game.h: xr.wargus2 = Wargus, xr.wargus1 = War1gus),
//  - einmalig VR-Voreinstellungen in die Einstellungen des Spielpakets geschrieben,
//  - die Engine mit -d <daten> -u <benutzer> gestartet.
//
// Datenordner: /sdcard/Android/data/<app-id>/files/data.Wargus bzw. data.War1gus
// (per adb befüllbar, keine Speicherberechtigung nötig).

#include "stratagus.h"

#include <SDL.h>
#include <android/log.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../xr/xr_game.h"
#include "../xr/xr_log.h"

namespace
{
constexpr const char *kLogTag = "Stratagus";
int gLogPipe[2] = {-1, -1};

void *LogThread(void *)
{
	char buf[1024];
	std::string line;
	ssize_t n;
	while ((n = read(gLogPipe[0], buf, sizeof(buf))) > 0) {
		line.append(buf, static_cast<size_t>(n));
		size_t pos;
		while ((pos = line.find('\n')) != std::string::npos) {
			xrs::logEngineLine(line.substr(0, pos).c_str());
			line.erase(0, pos + 1);
		}
	}
	return nullptr;
}

void RedirectStdioToLogcat()
{
	setvbuf(stdout, nullptr, _IOLBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);
	if (pipe(gLogPipe) != 0) {
		return;
	}
	dup2(gLogPipe[1], STDOUT_FILENO);
	dup2(gLogPipe[1], STDERR_FILENO);
	pthread_t thread;
	if (pthread_create(&thread, nullptr, LogThread, nullptr) == 0) {
		pthread_detach(thread);
	}
}

// Schließen der App → SDLActivity.onDestroy → SDL_APP_TERMINATING. Stratagus reagiert nur auf
// SDL_QUIT, und in einer laufenden Partie auch nur mit „zurück ins Menü"; Android tötet die
// App dann erst nach 10 s (destroyTimeout). Einstellungen speichert Wargus sofort bei jeder
// Änderung, daher hier direkt beenden. Läuft im Java-UI-Thread.
int SDLCALL OnAppEvent(void *, SDL_Event *event)
{
	if (event->type == SDL_APP_TERMINATING) {
		__android_log_print(ANDROID_LOG_INFO, kLogTag, "App wird beendet");
		_exit(0);
	}
	return 1;
}

// Fenster-Ereignisse des (in VR unsichtbaren) Android-Fensters verwerfen. Die immersive App hat
// aus Android-Sicht oft keinen Fensterfokus (der VR-Compositor hält ihn): Stratagus pausierte
// dann bei FOCUS_LOST das Spiel (PauseOnLeave) und meldete bei LEAVE „Maus hat das Fenster
// verlassen“. In VR bestimmt allein der OpenXR-Fokus, ob gespielt wird (xr_app.cpp).
int SDLCALL FilterWindowEvents(void *, SDL_Event *event)
{
	if (event->type == SDL_WINDOWEVENT) {
		switch (event->window.event) {
			case SDL_WINDOWEVENT_FOCUS_LOST:
			case SDL_WINDOWEVENT_FOCUS_GAINED:
			case SDL_WINDOWEVENT_ENTER:
			case SDL_WINDOWEVENT_LEAVE:
				return 0;
			default:
				break;
		}
	}
	return 1;
}

bool FileReadable(const std::string &path)
{
	return access(path.c_str(), R_OK) == 0;
}

// Einmalige VR-Voreinstellungen, je Stufe eine Marker-Datei (danach gehören die Werte dem
// Spieler; neue Stufen überschreiben ältere Werte nicht erneut). Spätere Zeilen überschreiben
// frühere; das Spiel schreibt die Datei beim Speichern neu. Stufen je Spielpaket: xr_game.h.
void SeedPreferences(const xrs::GameProfile &game, const std::string &userDir)
{
	const std::string gameDir = userDir + "/" + game.gameName;
	const std::string prefs = gameDir + "/preferences.lua";
	mkdir(gameDir.c_str(), 0755);
	for (int i = 0; i < game.stepCount; ++i) {
		const xrs::PreferenceStep &step = game.steps[i];
		const std::string marker = gameDir + "/" + step.marker;
		if (FileReadable(marker)) {
			continue;
		}
		const bool exists = FileReadable(prefs);
		FILE *f = fopen(prefs.c_str(), "a");
		if (!f) {
			return;
		}
		if (!exists) {
			fprintf(f,
			        "if (%s == nil) then %s = {} end\n"
			        "if (%s.preferences == nil) then %s.preferences = {} end\n",
			        game.prefsTable, game.prefsTable, game.prefsTable, game.prefsTable);
		}
		fprintf(f, "-- %s: VR-Voreinstellungen (%s)\n%s", game.appId, step.marker, step.lines);
		fclose(f);
		if (FILE *m = fopen(marker.c_str(), "w")) {
			fputs("1\n", m);
			fclose(m);
		}
		XLOGI("start", "VR-Voreinstellungen %s geschrieben", step.marker);
	}
}

// Ohne Spieldaten trotzdem ein Fenster zeigen: die XR-Schicht startet beim ersten Present und
// zeigt in der Brille, was fehlt und wie der Import geht (Ladeanzeige, Fehlerseite).
int ShowMissingData(const xrs::GameProfile &game)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	SDL_Window *window = SDL_CreateWindow(game.appId, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280,
	                                      800, SDL_WINDOW_FULLSCREEN);
	SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : nullptr;
	if (!renderer) {
		return 1;
	}
	for (;;) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				return 1;
			}
		}
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);
	}
}
} // namespace

int main(int argc, char *argv[])
{
	// Spielpaket nach App-ID (StratagusActivity.getArguments: "--xr-app=<id>"), Rest geht an die Engine.
	const char *appId = nullptr;
	std::vector<std::string> extraArgs;
	for (int i = 1; i < argc; ++i) {
		if (const char *id = xrs::appIdFromArg(argv[i])) {
			appId = id;
		} else {
			extraArgs.emplace_back(argv[i]);
		}
	}
	const xrs::GameProfile &game = xrs::gameProfileFor(appId);
	setenv("XRS_GAME", game.id, 1);

	const char *external = SDL_AndroidGetExternalStoragePath();
	const std::string base = external ? external : SDL_AndroidGetInternalStoragePath();
	const std::string dataDir = base + "/" + game.dataDirName;
	const std::string userDir = base + "/user";
	const std::string logDir = base + "/logs";

	mkdir(userDir.c_str(), 0755);
	setenv("HOME", userDir.c_str(), 1);
	setenv("XRS_DATA_DIR", dataDir.c_str(), 1);
	setenv("XRS_USER_DIR", userDir.c_str(), 1);
	setenv("XRS_LOG_DIR", logDir.c_str(), 1);

	RedirectStdioToLogcat();
	SDL_AddEventWatch(OnAppEvent, nullptr);
	SDL_SetEventFilter(FilterWindowEvents, nullptr);

	XLOGI("start", "%s (%s), Daten: %s, Benutzer: %s", game.appId, game.engineName, dataDir.c_str(), userDir.c_str());

	if (!FileReadable(dataDir + "/" + game.checkFile) || !FileReadable(dataDir + "/extracted")) {
		XLOGE("start", "Spieldaten nicht gefunden unter %s – Import: %s", dataDir.c_str(), game.importCommand);
		return ShowMissingData(game);
	}
	SeedPreferences(game, userDir);
	if (chdir(dataDir.c_str()) != 0) {
		XLOGE("start", "chdir(%s) fehlgeschlagen", dataDir.c_str());
	}

	std::vector<std::string> args = {"stratagus", "-d", dataDir, "-u", userDir};
	args.insert(args.end(), extraArgs.begin(), extraArgs.end());
	std::vector<char *> engineArgv;
	for (std::string &arg : args) {
		engineArgv.push_back(arg.data());
	}
	engineArgv.push_back(nullptr);

	return stratagusMain(static_cast<int>(args.size()), engineArgv.data());
}
