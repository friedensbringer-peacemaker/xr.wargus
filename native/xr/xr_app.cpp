// OpenXR-Host für Stratagus/Wargus (Aufbau siehe xr_app.h).
//
// Threads:
//   Spielthread (SDL/Engine): presentFrame() kopiert den fertigen Frame in einen von drei
//     geteilten Texturplätzen (FrameSlot) und wendet angeforderte Mausbewegungen an.
//   XR-Thread: eigener EGL-Kontext (geteilt mit SDL, ohne Fläche), alle OpenXR-Aufrufe,
//     Eingabe, Ladeanzeige, Menü, Tastatur, Strahl – mit der Bildrate der Brille.
// Übergabe über Mutex + GL-Fences (glFenceSync/glWaitSync, im Share-Group gültig).
//
// Ebenen (von hinten nach vorn): Passthrough (optional), Spielfenster bzw. Ladeanzeige (Quad),
// VR-Tastatur/VR-Menü (Quad, nur offen), Strahl + Zielpunkt (Stereo-Projection-Layer mit Alpha).
// Bedienung: docs/BEDIENUNG.md (bei Änderungen mitpflegen).

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES

#include "xr_app.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
// gl2ext.h braucht die Typen aus gl3.h (Reihenfolge nicht umsortieren).
#include <GLES2/gl2ext.h>
#include <SDL.h>
#include <jni.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <sys/stat.h>
#include <time.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "xr_game.h"
#include "xr_geometry.h"
#include "xr_gl.h"
#include "xr_input.h"
#include "xr_keyboard.h"
#include "xr_loader.h"
#include "xr_log.h"
#include "xr_math.h"
#include "xr_menu.h"
#include "xr_watchdog.h"
#include "xr_menu_model.h"
#include "xr_settings.h"
#include "xr_theme.h"

namespace xrs {
namespace {

// Fenster: Startmaße (Grenzen in xr_geometry.h).
constexpr float kDefaultWidth = 2.0f;
constexpr float kDefaultDistance = 1.8f;
constexpr float kBelowEyes = 0.15f;       // Fenstermitte etwas unter Augenhöhe
constexpr float kAdjustSpeed = 0.8f;      // m/s bei vollem Stickausschlag
constexpr float kMenuStep = 0.2f;         // m pro Klick auf „kleiner/größer/näher/weiter“
constexpr float kPointerTau = 0.025f;     // Zeigerglättung (leicht)
constexpr float kClickFreeze = 0.085f;    // Zielpunkt nach Triggerdruck einfrieren
constexpr float kHapticSeconds = 0.018f;
constexpr float kHapticAmplitude = 0.25f;
constexpr float kSaveDelay = 0.6f;        // Fenstermaße erst nach Ruhe speichern
constexpr float kSecondPulseDelay = 0.09f; // Doppelpuls „Beenden scharf“
constexpr float kMovieSkipDelay = 0.8f;   // Film läuft so lange, bevor er automatisch übersprungen wird

// Menü: hängt über dem Controller der Nicht-Zeigerhand (Standard links) und schaut zum Kopf.
constexpr float kMenuWidth = 0.46f;
constexpr float kMenuAboveHand = 0.22f;   // Mitte über dem Controller
constexpr float kMenuForward = 0.04f;     // leicht vor den Controller
constexpr float kMenuDistance = 0.6f;     // Rückfall ohne Handtracking: vor dem Kopf
constexpr float kMenuBelowEyes = 0.15f;

// VR-Tastatur: vor dem Spieler, unter Augenhöhe, Oberkante leicht nach hinten geneigt
constexpr float kKeyboardWidth = 1.0f;
constexpr float kKeyboardDistance = 0.8f;
constexpr float kKeyboardBelowEyes = 0.42f;
constexpr float kKeyboardPitch = -0.45f;
constexpr int kClearBackspaces = 32;   // „Name vorschlagen“ leert das Feld vorher

// Strahl
constexpr float kBeamLength = 3.0f;
constexpr float kBeamHalfWidth = 0.0018f;
constexpr float kEyeScale = 0.7f;         // Auflösung des Strahl-Layers relativ zur Empfehlung

// Ladeanzeige
constexpr float kStepMinSeconds = 0.4f;   // kurze Schritte lesbar lassen
constexpr float kGameStepSeconds = 0.6f;  // nach dem Startsignal noch erste Bilder abwarten
constexpr float kLoaderFallback = 30.0f;  // ohne Startsignal: spätestens dann Spielbild zeigen

// Statistik
constexpr double kStatsInterval = 10.0;
constexpr double kSlowFrameMs = 100.0;

constexpr int kSlots = 3;

enum { kLeft = 0, kRight = 1 };

uint64_t nowNs() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
}

const char* stateName(XrSessionState s) {
  switch (s) {
    case XR_SESSION_STATE_IDLE: return "IDLE";
    case XR_SESSION_STATE_READY: return "READY";
    case XR_SESSION_STATE_SYNCHRONIZED: return "SYNCHRONIZED";
    case XR_SESSION_STATE_VISIBLE: return "VISIBLE";
    case XR_SESSION_STATE_FOCUSED: return "FOCUSED";
    case XR_SESSION_STATE_STOPPING: return "STOPPING";
    case XR_SESSION_STATE_LOSS_PENDING: return "LOSS_PENDING";
    case XR_SESSION_STATE_EXITING: return "EXITING";
    default: return "UNKNOWN";
  }
}

const char* stepName(LoadStep s) {
  switch (s) {
    case LoadStep::kVr: return "VR";
    case LoadStep::kData: return "Daten";
    case LoadStep::kEngine: return "Engine";
    case LoadStep::kGame: return "Spielbild";
    case LoadStep::kDone: return "fertig";
  }
  return "?";
}

// ================================================================ geteilter Zustand

struct FrameSlot {
  GLuint texture = 0;
  GLuint gameFbo = 0;  // im SDL-Kontext
  GLuint xrFbo = 0;    // im XR-Kontext
  GLsync written = nullptr;
  GLsync readDone = nullptr;
};

struct Shared {
  std::mutex m;
  std::condition_variable cv;
  FrameSlot slots[kSlots];
  int latest = -1, reading = -1;
  uint64_t published = 0, consumed = 0;
  int frameW = 0, frameH = 0;

  std::atomic<int> windowW{0}, windowH{0};
  std::atomic<uint32_t> windowId{0};
  std::atomic<bool> warpPending{false};
  std::atomic<int> warpX{0}, warpY{0};

  std::atomic<bool> xrReady{false}, xrFailed{false}, xrRunning{false};

  // Engine-Signale
  std::atomic<bool> engineCursor{false};
  std::atomic<bool> movieRunning{false};
  std::atomic<bool> gameCursor{true};  // Spiel-Mauszeiger zeichnen (Einstellung, für native/src/cursor_hook.cpp)
  std::atomic<uint32_t> moviesStarted{0};
  std::mutex errorMutex;
  bool engineError = false;
  std::string engineErrorText, lastEngineLine;

  // Spiel-Statistik (Spielthread schreibt, XR-Thread liest)
  std::mutex statsMutex;
  uint64_t presents = 0, slowFrames = 0;
  double intervalSum = 0.0, intervalMax = 0.0;
  uint64_t lastPresentNs = 0, lastSlowWarnNs = 0;
};

Shared s;

// Nur Spielthread
struct GameThread {
  bool started = false;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLContext sdlContext = EGL_NO_CONTEXT;
  EGLContext xrContext = EGL_NO_CONTEXT;
  EGLConfig config = nullptr;
  JavaVM* vm = nullptr;
  jobject activity = nullptr;
} gt;

// ================================================================ XR-Thread-Zustand

struct Swapchain {
  XrSwapchain handle = XR_NULL_HANDLE;
  int32_t width = 0, height = 0;
  std::vector<XrSwapchainImageOpenGLESKHR> images;
  std::vector<GLuint> framebuffers;
  std::vector<uint32_t> uploadedVersion;  // CPU-Inhalte: welcher Stand liegt in welchem Bild
};

// Gehaltene Tasten (Scrollen, Modifikatoren), damit sie gezielt gelöst werden können.
struct Keys {
  bool up = false, down = false, left = false, right = false, shift = false, ctrl = false;
};

struct State {
  XrInstance instance = XR_NULL_HANDLE;
  XrSystemId systemId = XR_NULL_SYSTEM_ID;
  XrSession session = XR_NULL_HANDLE;
  XrSpace localSpace = XR_NULL_HANDLE;
  XrSpace viewSpace = XR_NULL_HANDLE;
  XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
  bool running = false;
  bool srgbWriteControl = false;
  int64_t colorFormat = 0;

  Swapchain game, menu, loader, keyboard, eyes[2];
  BeamRenderer beam;
  bool beamReady = false;
  FilterRenderer filter;
  bool filterReady = false;

  // Passthrough (XR_FB_passthrough, optional)
  bool passthroughAvailable = false;
  bool passthroughRunning = false;
  XrPassthroughFB passthrough = XR_NULL_HANDLE;
  XrPassthroughLayerFB passthroughLayer = XR_NULL_HANDLE;
  PFN_xrCreatePassthroughFB xrCreatePassthroughFB_ = nullptr;
  PFN_xrPassthroughStartFB xrPassthroughStartFB_ = nullptr;
  PFN_xrPassthroughPauseFB xrPassthroughPauseFB_ = nullptr;
  PFN_xrCreatePassthroughLayerFB xrCreatePassthroughLayerFB_ = nullptr;
  PFN_xrPassthroughLayerResumeFB xrPassthroughLayerResumeFB_ = nullptr;
  PFN_xrPassthroughLayerPauseFB xrPassthroughLayerPauseFB_ = nullptr;

  // VR-Tastatur (über das VR-Menü geöffnet: Stratagus hat Texteingabe dauerhaft an)
  KeyboardPanel keyboardPanel;
  bool keyboardReady = false;
  bool keyboardOpen = false;
  XrPosef keyboardPose{{0, 0, 0, 1}, {0, 0, 0}};
  AnalogButton keyClick;

  XrActionSet actionSet = XR_NULL_HANDLE;
  XrAction aimPose = XR_NULL_HANDLE, trigger = XR_NULL_HANDLE, grip = XR_NULL_HANDLE,
           stick = XR_NULL_HANDLE, stickClick = XR_NULL_HANDLE, buttonA = XR_NULL_HANDLE,
           buttonB = XR_NULL_HANDLE, buttonX = XR_NULL_HANDLE, buttonY = XR_NULL_HANDLE,
           menuButton = XR_NULL_HANDLE, haptic = XR_NULL_HANDLE;
  XrPath hand[2] = {XR_NULL_PATH, XR_NULL_PATH};
  XrSpace aimSpace[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
  std::string profile = "(keins)";

  Settings settings;

  // Spielfenster: eine Pose für Compositor und Treffer.
  bool centered = false;
  float yaw = 0.0f;
  XrVector3f anchor{0.0f, 0.0f, 0.0f};
  XrPosef head{{0, 0, 0, 1}, {0, 0, 0}};
  bool headValid = false;
  float saveTimer = -1.0f;

  // Menü
  VrMenu vrMenu;
  bool menuReady = false;
  bool menuOpen = false;
  XrPosef menuPose{{0, 0, 0, 1}, {0, 0, 0}};
  int diagnoseCount = 0;
  std::string menuNote;
  GroupMode groupMode = GroupMode::kSelect;
  ConfirmAction quitConfirm;  // Beenden: erster Klick scharf, zweiter in 3 s beendet
  float secondPulse = -1.0f;  // Doppelpuls beim Scharfschalten: Restzeit bis zum zweiten Impuls
  bool quitting = false;

  // Ladeanzeige
  LoaderPanel loaderPanel;
  LoaderState load;
  bool loaderReady = false;
  bool loaderDismissed = false;  // Fehleranzeige per B weggeklickt
  bool hasGameFrame = false;

  // Filme
  uint32_t moviesSeen = 0;
  float movieSkipIn = -1.0f;  // automatisch überspringen in … s (< 0: nicht geplant)
  int movieSkipTries = 0;

  // Eingabe
  XrTime lastDisplayTime = 0;
  XrTime frameTime = 0;  // vorhergesagte Anzeigezeit des laufenden Frames
  AnalogButton leftClick, rightClick, adjustGrip, menuClick;
  PressButton pressB, pressY, pressMenu, pressAdjustStick, pressStick;
  // Menü per Stick/A (P2): Fokus-Wiederholung, Auslösen mit Dauerdruck für ‹ ›-Aktionen.
  HoldRepeat menuStick, menuTrigger, menuA;
  int menuStickDx = 0, menuStickDy = 0;
  int menuRayHover = -1;      // Schaltfläche unter dem Strahl im letzten Frame
  bool menuStickFocus = false;  // Fokus kommt vom Stick (Strahl übernimmt erst, wenn er sich bewegt)
  bool shiftNeedsRelease = false;  // A hat im Menü ausgelöst: erst loslassen, dann wieder Umschalt
  bool adjusting = false;
  bool pointerValid = false;
  float pointerU = 0.5f, pointerV = 0.5f;
  float freezeLeft = 0.0f;
  Keys keys;
  std::vector<SDL_Keycode> pendingKeyUps;
  uint64_t clicks = 0, keyTaps = 0, pointerFrames = 0, pointerHits = 0;

  // Strahl dieses Frames
  bool beamVisible = false;
  XrVector3f beamStart{}, beamEnd{};
  bool beamHit = false;

  // XR-Statistik
  uint64_t xrFrames = 0, notRendered = 0;
  double waitSum = 0.0, workSum = 0.0;
  uint64_t statsStartNs = 0;
  double lastFps = 0.0, lastXrHz = 0.0;
};

State g;

int pointerHand() { return g.settings.pointerHand ? kRight : kLeft; }
int otherHand() { return 1 - pointerHand(); }

bool ok(XrResult r, const char* what) {
  if (XR_SUCCEEDED(r)) return true;
  char text[XR_MAX_RESULT_STRING_SIZE] = {};
  if (g.instance != XR_NULL_HANDLE) {
    xrResultToString(g.instance, r, text);
  } else {
    std::snprintf(text, sizeof(text), "%d", static_cast<int>(r));
  }
  XLOGE("xr", "%s fehlgeschlagen: %s (%d)", what, text, static_cast<int>(r));
  return false;
}

XrPath path(const char* p) {
  XrPath result = XR_NULL_PATH;
  xrStringToPath(g.instance, p, &result);
  return result;
}

bool loaderVisible() {
  if (g.load.step == LoadStep::kDone) return false;
  if ((g.load.error || g.load.dataMissing) && g.loaderDismissed) return false;
  return true;
}

// ================================================================ SDL-Ereignisse (XR-Thread)

std::string keyName(SDL_Keycode key) {
  switch (key) {
    case SDLK_UP: return "Hoch";
    case SDLK_DOWN: return "Runter";
    case SDLK_LEFT: return "Links";
    case SDLK_RIGHT: return "Rechts";
    case SDLK_LSHIFT: return "Umschalt";
    case SDLK_LCTRL: return "Strg";
    case SDLK_ESCAPE: return "Escape";
    case SDLK_RETURN: return "Enter";
    case SDLK_PAUSE: return "Pause";
    case SDLK_BACKSPACE: return "Rück";
    case SDLK_PRINTSCREEN: return "Druck";
    default: break;
  }
  if (key >= SDLK_F1 && key <= SDLK_F12) return "F" + std::to_string(key - SDLK_F1 + 1);
  if (key >= 0x20 && key < 0x7F) return std::string(1, static_cast<char>(key));
  char text[16];
  std::snprintf(text, sizeof(text), "0x%x", static_cast<unsigned>(key));
  return text;
}

void sendKey(SDL_Keycode key, bool down) {
  SDL_Event e;
  SDL_zero(e);
  e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
  e.key.windowID = s.windowId.load();
  e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.key.keysym.sym = key;
  e.key.keysym.scancode = SDL_GetScancodeFromKey(key);
  e.key.keysym.mod = static_cast<Uint16>((g.keys.shift ? KMOD_LSHIFT : 0) | (g.keys.ctrl ? KMOD_LCTRL : 0));
  SDL_PushEvent(&e);
}

// Druckbares ASCII-Zeichen als Texteingabe. Stratagus wertet druckbare Tasten ohne Strg/Alt
// nur über SDL_TEXTINPUT aus (sdl.cpp isTextInput) – dort auch als Hotkey („.“, Leertaste …).
void sendText(const std::string& text) {
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_TEXTINPUT;
  e.text.windowID = s.windowId.load();
  std::snprintf(e.text.text, sizeof(e.text.text), "%s", text.c_str());
  SDL_PushEvent(&e);
  if (inputLogging()) XLOGD("input", "Text \"%s\"", text.c_str());
}

// Kurzer Tastendruck (Spiel-Hotkey): jetzt drücken, im nächsten Frame loslassen.
void tapKey(SDL_Keycode key) {
  sendKey(key, true);
  g.pendingKeyUps.push_back(key);
  ++g.keyTaps;
  XLOGI("input", "Taste %s", keyName(key).c_str());
}

void tapChar(char c) {
  sendText(std::string(1, c));
  ++g.keyTaps;
  XLOGI("input", "Zeichen '%c'", c);
}

void tapKeyQuiet(SDL_Keycode key) {
  sendKey(key, true);
  sendKey(key, false);
}

// Taste mit Modifikatoren (Strg/Umschalt als eigene Tastenereignisse – Stratagus führt die
// Modifikatoren selbst aus KEYDOWN/KEYUP der Modifikatortasten).
void tapWithModifiers(SDL_Keycode key, bool ctrl, bool shift) {
  bool hadCtrl = g.keys.ctrl, hadShift = g.keys.shift;
  if (ctrl && !hadCtrl) { g.keys.ctrl = true; sendKey(SDLK_LCTRL, true); }
  if (shift && !hadShift) { g.keys.shift = true; sendKey(SDLK_LSHIFT, true); }
  sendKey(key, true);
  sendKey(key, false);
  if (shift && !hadShift) { g.keys.shift = false; sendKey(SDLK_LSHIFT, false); }
  if (ctrl && !hadCtrl) { g.keys.ctrl = false; sendKey(SDLK_LCTRL, false); }
  ++g.keyTaps;
  XLOGI("input", "Taste %s%s%s", ctrl ? "Strg+" : "", shift ? "Umschalt+" : "", keyName(key).c_str());
}

// Text zeichenweise tippen (nur ASCII kommt bei Stratagus an).
void typeText(const std::string& text) {
  for (char c : text) {
    if (static_cast<unsigned char>(c) < 0x80) sendText(std::string(1, c));
  }
}

// Zeitstempel als Name (Spielstand) – Feld vorher leeren.
void typeSuggestion() {
  for (int i = 0; i < kClearBackspaces; ++i) tapKeyQuiet(SDLK_BACKSPACE);
  time_t now = time(nullptr);
  tm local{};
  localtime_r(&now, &local);
  char name[32];
  strftime(name, sizeof(name), "%Y-%m-%d_%H-%M", &local);
  typeText(name);
  XLOGI("keyboard", "Name vorgeschlagen: %s", name);
}

void pointerToWindow(int* x, int* y) {
  *x = static_cast<int>(std::clamp(g.pointerU, 0.0f, 1.0f) * static_cast<float>(s.windowW.load() - 1));
  *y = static_cast<int>(std::clamp(g.pointerV, 0.0f, 1.0f) * static_cast<float>(s.windowH.load() - 1));
}

void sendMouseButton(Uint8 button, bool down) {
  int x, y;
  pointerToWindow(&x, &y);
  SDL_Event e;
  SDL_zero(e);
  e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
  e.button.windowID = s.windowId.load();
  e.button.button = button;
  e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.button.clicks = 1;
  e.button.x = x;
  e.button.y = y;
  SDL_PushEvent(&e);
  if (down) ++g.clicks;
  if (inputLogging()) {
    XLOGD("input", "Maus %s %s bei %d,%d (u=%.3f v=%.3f)", button == SDL_BUTTON_LEFT ? "links" : "rechts",
          down ? "runter" : "hoch", x, y, g.pointerU, g.pointerV);
  }
}

// Mausposition: SDL_WarpMouseInWindow gehört in den Spielthread – dort ausgeführt.
void requestWarp() {
  int x, y;
  pointerToWindow(&x, &y);
  s.warpX.store(x);
  s.warpY.store(y);
  s.warpPending.store(true);
}

void setHeld(bool& held, bool want, SDL_Keycode key) {
  if (held != want) {
    held = want;
    sendKey(key, want);
    if (inputLogging()) XLOGD("input", "Taste %s %s", keyName(key).c_str(), want ? "gehalten" : "gelöst");
  }
}

void releaseScroll() {
  setHeld(g.keys.up, false, SDLK_UP);
  setHeld(g.keys.down, false, SDLK_DOWN);
  setHeld(g.keys.left, false, SDLK_LEFT);
  setHeld(g.keys.right, false, SDLK_RIGHT);
}

// Gehaltene Spiel-Eingaben lösen (Fokus-/Trackingverlust, Menü, Handwechsel) und re-armen.
void releaseAll(const char* reason) {
  bool any = g.leftClick.down() || g.rightClick.down() || g.keys.up || g.keys.down || g.keys.left ||
             g.keys.right || g.keys.shift || g.keys.ctrl;
  if (g.leftClick.down()) sendMouseButton(SDL_BUTTON_LEFT, false);
  if (g.rightClick.down()) sendMouseButton(SDL_BUTTON_RIGHT, false);
  g.leftClick.reset();
  g.rightClick.reset();
  g.adjustGrip.reset();
  g.menuClick.reset();
  releaseScroll();
  setHeld(g.keys.shift, false, SDLK_LSHIFT);
  setHeld(g.keys.ctrl, false, SDLK_LCTRL);
  for (SDL_Keycode k : g.pendingKeyUps) sendKey(k, false);
  g.pendingKeyUps.clear();
  g.pressB.reset();
  g.pressY.reset();
  g.pressMenu.reset();
  g.pressAdjustStick.reset();
  g.pressStick.reset();
  g.freezeLeft = 0.0f;
  if (any) XLOGI("input", "gehaltene Eingaben gelöst (%s)", reason);
}

// ================================================================ Fenster-Pose

XrPosef panelPose() { return panelPoseFor(g.anchor, g.yaw, g.settings.distance); }

float panelHeight() {
  return g.game.width > 0 ? g.settings.width * static_cast<float>(g.game.height) / static_cast<float>(g.game.width)
                          : g.settings.width * 0.625f;
}

float menuHeight() { return kMenuWidth * static_cast<float>(VrMenu::kHeight) / static_cast<float>(VrMenu::kWidth); }

void clampPanel() { clampPanelSize(&g.settings.width, &g.settings.distance); }

// Start/Neuzentrierung: nur Yaw aus der Kopfrotation, Pitch nicht.
void recenter(const XrPosef& head) {
  g.yaw = yawOf(head.orientation);
  g.anchor = head.position;
  g.anchor.y -= kBelowEyes;
  clampPanel();
  g.centered = true;
  XLOGI("window", "zentriert: Breite %.2f m, Abstand %.2f m, Richtung %.0f°, Kopf %.2f/%.2f/%.2f",
        g.settings.width, g.settings.distance, g.yaw * 57.2958f, head.position.x, head.position.y,
        head.position.z);
}

void pulse() {
  XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
  info.action = g.haptic;
  info.subactionPath = g.hand[pointerHand()];
  XrHapticVibration vib{XR_TYPE_HAPTIC_VIBRATION};
  vib.duration = static_cast<XrDuration>(kHapticSeconds * 1e9f);
  vib.frequency = XR_FREQUENCY_UNSPECIFIED;
  vib.amplitude = kHapticAmplitude;
  xrApplyHapticFeedback(g.session, &info, reinterpret_cast<XrHapticBaseHeader*>(&vib));
}

// P3: Strahl, Zielpunkt und Spiel-Mauszeiger je abschaltbar; im offenen Menü/Tastatur bleiben Strahl
// und Zielpunkt sichtbar.
PointerVisibility pointerVisibility() {
  PointerVisibility v;
  v.ray = g.settings.beam;
  v.marker = g.settings.target != TargetStyle::kOff;
  v.gameCursor = g.settings.gameCursor;
  return v;
}

bool overlayOpen() { return g.menuOpen || g.keyboardOpen; }

void saveSoon() { g.saveTimer = kSaveDelay; }

// ================================================================ Diagnose und Statistik

std::string statusLine() {
  char text[256];
  std::snprintf(text, sizeof(text), "Spiel %.0f fps - XR %.0f Hz - Fenster %.1f m, %.1f m entfernt%s%s", g.lastFps,
                g.lastXrHz, g.settings.width, g.settings.distance, g.menuNote.empty() ? "" : " - ",
                g.menuNote.c_str());
  return text;
}

void logDiagnose() {
  ++g.diagnoseCount;
  XrPosef panel = panelPose();
  XLOGI("diagnose", "===== DIAGNOSE #%d =====", g.diagnoseCount);
  XLOGI("diagnose", "Session %s, Profil %s, Zeigerhand %s, Ladeanzeige %s", stateName(g.sessionState),
        g.profile.c_str(), g.settings.pointerHand ? "rechts" : "links", stepName(g.load.step));
  XLOGI("diagnose", "Fenster: Breite %.2f m, Höhe %.2f m, Abstand %.2f m, Mitte %.2f/%.2f/%.2f, Richtung %.0f°",
        g.settings.width, panelHeight(), g.settings.distance, panel.position.x, panel.position.y,
        panel.position.z, g.yaw * 57.2958f);
  XLOGI("diagnose", "Kopf %s %.2f/%.2f/%.2f", g.headValid ? "gültig" : "ungültig", g.head.position.x,
        g.head.position.y, g.head.position.z);
  XLOGI("diagnose", "SDL-Fenster %dx%d, Spielbild %dx%d, Format 0x%llx, sRGB-Durchreichung %s",
        s.windowW.load(), s.windowH.load(), g.game.width, g.game.height,
        static_cast<long long>(g.colorFormat), g.srgbWriteControl ? "ja" : "nein");
  XLOGI("diagnose", "Zeiger u=%.3f v=%.3f gültig=%d, Klicks %llu, Tasten %llu, Filme %u, Film läuft %d",
        g.pointerU, g.pointerV, g.pointerValid, static_cast<unsigned long long>(g.clicks),
        static_cast<unsigned long long>(g.keyTaps), s.moviesStarted.load(), s.movieRunning.load());
  XLOGI("diagnose", "Leistung: Spiel %.1f fps, XR %.1f Hz", g.lastFps, g.lastXrHz);
  XLOGI("diagnose", "Einstellungen: Strahl %d, Zielpunkt %d, Eingabelog %d, Filme überspringen %d, Menü %s",
        g.settings.beam, static_cast<int>(g.settings.target), g.settings.logInput, g.settings.autoSkipMovies,
        g.settings.menuOrc ? "Ork" : "Mensch");
}

void updateStats() {
  uint64_t now = nowNs();
  {
    uint64_t lastPresent;
    {
      std::lock_guard<std::mutex> lock(s.statsMutex);
      lastPresent = s.lastPresentNs;
    }
    watchdogPoll(now, lastPresent, s.movieRunning.load() ? "Film läuft" : "kein Film");
  }
  if (!g.statsStartNs) g.statsStartNs = now;
  double elapsed = static_cast<double>(now - g.statsStartNs) * 1e-9;
  uint64_t presents;
  {
    std::lock_guard<std::mutex> lock(s.statsMutex);
    presents = s.presents;
  }
  if (elapsed >= 1.0) {
    g.lastFps = static_cast<double>(presents) / elapsed;
    g.lastXrHz = static_cast<double>(g.xrFrames) / elapsed;
  }
  if (elapsed < kStatsInterval) return;
  double intervalSum, intervalMax;
  uint64_t slow;
  {
    std::lock_guard<std::mutex> lock(s.statsMutex);
    presents = s.presents;
    intervalSum = s.intervalSum;
    intervalMax = s.intervalMax;
    slow = s.slowFrames;
    s.presents = 0;
    s.intervalSum = 0.0;
    s.intervalMax = 0.0;
  }
  double xf = static_cast<double>(std::max<uint64_t>(g.xrFrames, 1));
  XLOGI("stats",
        "Spiel %.1f fps (Abstand ⌀ %.1f / max %.1f ms, langsame gesamt %llu), XR %.1f Hz, nicht gerendert %llu, "
        "xrWaitFrame ⌀ %.2f ms, XR-Arbeit ⌀ %.2f ms, Zeiger auf Fenster %.0f%%, Klicks %llu, Tasten %llu, "
        "Menü %s, Ladeanzeige %s",
        presents / elapsed, presents ? intervalSum / static_cast<double>(presents) : 0.0, intervalMax,
        static_cast<unsigned long long>(slow), g.xrFrames / elapsed,
        static_cast<unsigned long long>(g.notRendered), g.waitSum / xf, g.workSum / xf,
        g.pointerFrames ? 100.0 * static_cast<double>(g.pointerHits) / static_cast<double>(g.pointerFrames) : 0.0,
        static_cast<unsigned long long>(g.clicks), static_cast<unsigned long long>(g.keyTaps),
        g.menuOpen ? "offen" : "zu", stepName(g.load.step));
  g.xrFrames = 0;
  g.notRendered = 0;
  g.waitSum = 0.0;
  g.workSum = 0.0;
  g.pointerFrames = 0;
  g.pointerHits = 0;
  g.statsStartNs = now;
}

// ================================================================ Ladeanzeige

void setStep(LoadStep step) {
  if (g.load.step == step) return;
  XLOGI("loader", "%s -> %s nach %.1f s", stepName(g.load.step), stepName(step), g.load.seconds);
  g.load.step = step;
  g.load.stepSeconds = 0.0f;
  if (step == LoadStep::kDone) XLOGI("loader", "Spiel sichtbar nach %.1f s", g.load.seconds);
}

void updateLoader(float dt) {
  if (g.load.step == LoadStep::kDone) return;
  g.load.seconds += dt;
  g.load.stepSeconds += dt;
  {
    std::lock_guard<std::mutex> lock(s.errorMutex);
    if (s.engineError && !g.load.error) {
      g.load.error = true;
      g.load.errorText = s.engineErrorText;
      XLOGW("loader", "Engine-Fehler beim Start – Anzeige in der Brille: %s", g.load.errorText.c_str());
    }
  }
  bool started = s.moviesStarted.load() > 0 || s.engineCursor.load();
  switch (g.load.step) {
    case LoadStep::kVr:
      if (g.load.stepSeconds >= kStepMinSeconds) setStep(LoadStep::kData);
      break;
    case LoadStep::kData:
      if (!g.load.dataMissing && g.load.stepSeconds >= kStepMinSeconds) setStep(LoadStep::kEngine);
      break;
    case LoadStep::kEngine:
      if (started) {
        XLOGI("loader", "Startsignal der Engine: %s", s.moviesStarted.load() ? "Film" : "Mauszeiger");
        setStep(LoadStep::kGame);
      } else if (g.load.seconds >= kLoaderFallback && g.hasGameFrame && !g.load.error) {
        XLOGW("loader", "kein Startsignal nach %.0f s – zeige Spielbild", g.load.seconds);
        setStep(LoadStep::kDone);
      }
      break;
    case LoadStep::kGame:
      if (g.load.stepSeconds >= kGameStepSeconds && g.hasGameFrame) setStep(LoadStep::kDone);
      break;
    case LoadStep::kDone:
      break;
  }
}

// ================================================================ Initialisierung (XR-Thread)

bool createActions() {
  XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
  std::strcpy(setInfo.actionSetName, "gameplay");
  std::strcpy(setInfo.localizedActionSetName, "Gameplay");
  if (!ok(xrCreateActionSet(g.instance, &setInfo, &g.actionSet), "xrCreateActionSet")) return false;

  g.hand[kLeft] = path("/user/hand/left");
  g.hand[kRight] = path("/user/hand/right");

  auto make = [](XrAction* out, XrActionType type, const char* name, bool perHand) {
    XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
    info.actionType = type;
    std::strcpy(info.actionName, name);
    std::strcpy(info.localizedActionName, name);
    if (perHand) {
      info.countSubactionPaths = 2;
      info.subactionPaths = g.hand;
    }
    return ok(xrCreateAction(g.actionSet, &info, out), name);
  };
  if (!make(&g.aimPose, XR_ACTION_TYPE_POSE_INPUT, "aim", true) ||
      !make(&g.trigger, XR_ACTION_TYPE_FLOAT_INPUT, "trigger", true) ||
      !make(&g.grip, XR_ACTION_TYPE_FLOAT_INPUT, "grip", true) ||
      !make(&g.stick, XR_ACTION_TYPE_VECTOR2F_INPUT, "stick", true) ||
      !make(&g.stickClick, XR_ACTION_TYPE_BOOLEAN_INPUT, "stick_click", true) ||
      !make(&g.buttonA, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", false) ||
      !make(&g.buttonB, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", false) ||
      !make(&g.buttonX, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", false) ||
      !make(&g.buttonY, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", false) ||
      !make(&g.menuButton, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", false) ||
      !make(&g.haptic, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", true)) {
    return false;
  }

  std::vector<XrActionSuggestedBinding> touch = {
      {g.aimPose, path("/user/hand/left/input/aim/pose")},
      {g.aimPose, path("/user/hand/right/input/aim/pose")},
      {g.trigger, path("/user/hand/left/input/trigger/value")},
      {g.trigger, path("/user/hand/right/input/trigger/value")},
      {g.grip, path("/user/hand/left/input/squeeze/value")},
      {g.grip, path("/user/hand/right/input/squeeze/value")},
      {g.stick, path("/user/hand/left/input/thumbstick")},
      {g.stick, path("/user/hand/right/input/thumbstick")},
      {g.stickClick, path("/user/hand/left/input/thumbstick/click")},
      {g.stickClick, path("/user/hand/right/input/thumbstick/click")},
      {g.buttonA, path("/user/hand/right/input/a/click")},
      {g.buttonB, path("/user/hand/right/input/b/click")},
      {g.buttonX, path("/user/hand/left/input/x/click")},
      {g.buttonY, path("/user/hand/left/input/y/click")},
      {g.menuButton, path("/user/hand/left/input/menu/click")},
      {g.haptic, path("/user/hand/left/output/haptic")},
      {g.haptic, path("/user/hand/right/output/haptic")},
  };
  XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
  suggested.interactionProfile = path("/interaction_profiles/oculus/touch_controller");
  suggested.countSuggestedBindings = static_cast<uint32_t>(touch.size());
  suggested.suggestedBindings = touch.data();
  if (!ok(xrSuggestInteractionProfileBindings(g.instance, &suggested), "Bindings touch_controller")) {
    return false;
  }

  std::vector<XrActionSuggestedBinding> simple = {
      {g.aimPose, path("/user/hand/left/input/aim/pose")},
      {g.aimPose, path("/user/hand/right/input/aim/pose")},
      {g.trigger, path("/user/hand/left/input/select/click")},
      {g.trigger, path("/user/hand/right/input/select/click")},
      {g.menuButton, path("/user/hand/left/input/menu/click")},
      {g.haptic, path("/user/hand/left/output/haptic")},
      {g.haptic, path("/user/hand/right/output/haptic")},
  };
  suggested.interactionProfile = path("/interaction_profiles/khr/simple_controller");
  suggested.countSuggestedBindings = static_cast<uint32_t>(simple.size());
  suggested.suggestedBindings = simple.data();
  ok(xrSuggestInteractionProfileBindings(g.instance, &suggested), "Bindings simple_controller");
  return true;
}

void chooseColorFormat() {
  uint32_t count = 0;
  xrEnumerateSwapchainFormats(g.session, 0, &count, nullptr);
  std::vector<int64_t> formats(count);
  xrEnumerateSwapchainFormats(g.session, count, &count, formats.data());
  auto has = [&](int64_t f) { return std::find(formats.begin(), formats.end(), f) != formats.end(); };

  // Alle Inhalte (Spielbild, Menü, Ladeanzeige, Strahlfarben) sind sRGB-kodiert. Mit sRGB-
  // Swapchain und abgeschalteter Schreibkonvertierung (EXT_sRGB_write_control) gehen sie
  // bitgenau durch.
  const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  bool writeControl = ext && std::strstr(ext, "GL_EXT_sRGB_write_control");
  if (writeControl && has(GL_SRGB8_ALPHA8)) {
    g.colorFormat = GL_SRGB8_ALPHA8;
    g.srgbWriteControl = true;
    glDisable(GL_FRAMEBUFFER_SRGB_EXT);  // eigener Kontext: gilt für alle Zeichenvorgänge hier
  } else if (has(GL_RGBA8)) {
    g.colorFormat = GL_RGBA8;
    XLOGW("xr", "ohne sRGB-Durchreichung: Farben können blasser wirken");
  } else if (!formats.empty()) {
    g.colorFormat = formats[0];
  }
  XLOGI("xr", "Farbformat 0x%llx, sRGB-Durchreichung %s", static_cast<long long>(g.colorFormat),
        g.srgbWriteControl ? "ja" : "nein");
}

bool createSwapchain(Swapchain& sc, int width, int height, const char* name) {
  XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
  info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
                    XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
  info.format = g.colorFormat;
  info.sampleCount = 1;
  info.width = static_cast<uint32_t>(width);
  info.height = static_cast<uint32_t>(height);
  info.faceCount = 1;
  info.arraySize = 1;
  info.mipCount = 1;
  if (!ok(xrCreateSwapchain(g.session, &info, &sc.handle), name)) return false;
  sc.width = width;
  sc.height = height;

  uint32_t imageCount = 0;
  xrEnumerateSwapchainImages(sc.handle, 0, &imageCount, nullptr);
  sc.images.assign(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
  xrEnumerateSwapchainImages(sc.handle, imageCount, &imageCount,
                             reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data()));
  sc.framebuffers.assign(imageCount, 0);
  sc.uploadedVersion.assign(imageCount, 0);
  glGenFramebuffers(static_cast<GLsizei>(imageCount), sc.framebuffers.data());
  for (uint32_t i = 0; i < imageCount; ++i) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sc.framebuffers[i]);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sc.images[i].image, 0);
    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      XLOGE("xr", "Swapchain %s: FBO %u unvollständig", name, i);
    }
  }
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  XLOGI("xr", "Swapchain %s: %dx%d, %u Bilder", name, width, height, imageCount);
  return true;
}

void logRuntimeInfo() {
  XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
  if (XR_SUCCEEDED(xrGetInstanceProperties(g.instance, &props))) {
    XLOGI("xr", "Runtime %s %u.%u.%u", props.runtimeName, XR_VERSION_MAJOR(props.runtimeVersion),
          XR_VERSION_MINOR(props.runtimeVersion), XR_VERSION_PATCH(props.runtimeVersion));
  }
  XrSystemProperties sys{XR_TYPE_SYSTEM_PROPERTIES};
  if (XR_SUCCEEDED(xrGetSystemProperties(g.instance, g.systemId, &sys))) {
    XLOGI("xr", "System %s, max. Layer %u", sys.systemName, sys.graphicsProperties.maxLayerCount);
  }
  XLOGI("gl", "%s / %s / %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));
}

void checkGameData() {
  const char* dir = std::getenv("XRS_DATA_DIR");
  std::string file = std::string(dir ? dir : ".") + "/" + currentGame().checkFile;
  struct stat st{};
  g.load.dataMissing = stat(file.c_str(), &st) != 0;
  if (g.load.dataMissing) {
    XLOGE("loader", "Spieldaten fehlen: %s", file.c_str());
  } else {
    XLOGI("loader", "Spieldaten vorhanden (%s)", file.c_str());
  }
}

// Vorwärtsdeklaration (Definition bei den Menüaktionen).
void applyPassthrough();

bool initialize() {
  XLOGI("xr", "Initialisierung im XR-Thread, Logdatei %s", logFilePath());
  g.settings = loadSettings();
  s.gameCursor = g.settings.gameCursor;
  setInputLogging(g.settings.logInput);
  clampPanel();
  g.load.logPath = logFilePath();
  checkGameData();
  Theme::get().setOrc(g.settings.menuOrc);

  PFN_xrInitializeLoaderKHR initLoader = nullptr;
  xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                        reinterpret_cast<PFN_xrVoidFunction*>(&initLoader));
  if (initLoader) {
    XrLoaderInitInfoAndroidKHR loaderInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    loaderInfo.applicationVM = gt.vm;
    loaderInfo.applicationContext = gt.activity;
    if (!ok(initLoader(reinterpret_cast<XrLoaderInitInfoBaseHeaderKHR*>(&loaderInfo)),
            "xrInitializeLoaderKHR")) {
      return false;
    }
  }

  // Passthrough nur anfordern, wenn die Runtime es anbietet.
  uint32_t extCount = 0;
  xrEnumerateInstanceExtensionProperties(nullptr, 0, &extCount, nullptr);
  std::vector<XrExtensionProperties> extProps(extCount, {XR_TYPE_EXTENSION_PROPERTIES});
  xrEnumerateInstanceExtensionProperties(nullptr, extCount, &extCount, extProps.data());
  for (const XrExtensionProperties& p : extProps) {
    if (!std::strcmp(p.extensionName, XR_FB_PASSTHROUGH_EXTENSION_NAME)) g.passthroughAvailable = true;
  }
  std::vector<const char*> extensions = {XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
                                         XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME};
  if (g.passthroughAvailable) extensions.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
  XLOGI("xr", "Passthrough-Erweiterung %s", g.passthroughAvailable ? "vorhanden" : "fehlt");
  XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
  androidInfo.applicationVM = gt.vm;
  androidInfo.applicationActivity = gt.activity;
  XrInstanceCreateInfo instanceInfo{XR_TYPE_INSTANCE_CREATE_INFO};
  instanceInfo.next = &androidInfo;
  std::snprintf(instanceInfo.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "%s",
                currentGame().appId);
  instanceInfo.applicationInfo.applicationVersion = 2;
  std::strcpy(instanceInfo.applicationInfo.engineName, "Stratagus");
  instanceInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;
  instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  instanceInfo.enabledExtensionNames = extensions.data();
  if (!ok(xrCreateInstance(&instanceInfo, &g.instance), "xrCreateInstance")) return false;

  XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
  systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  if (!ok(xrGetSystem(g.instance, &systemInfo, &g.systemId), "xrGetSystem")) return false;
  logRuntimeInfo();

  // Vom Runtime verlangt: Grafikanforderungen vor xrCreateSession abfragen.
  PFN_xrGetOpenGLESGraphicsRequirementsKHR getReq = nullptr;
  xrGetInstanceProcAddr(g.instance, "xrGetOpenGLESGraphicsRequirementsKHR",
                        reinterpret_cast<PFN_xrVoidFunction*>(&getReq));
  XrGraphicsRequirementsOpenGLESKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
  if (!getReq || !ok(getReq(g.instance, g.systemId, &req), "GLES-Anforderungen")) return false;

  // Session mit dem eigenen XR-Kontext (geteilt mit SDL, in diesem Thread aktuell).
  XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
  binding.display = gt.display;
  binding.config = gt.config;
  binding.context = gt.xrContext;
  XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
  sessionInfo.next = &binding;
  sessionInfo.systemId = g.systemId;
  if (!ok(xrCreateSession(g.instance, &sessionInfo, &g.session), "xrCreateSession")) return false;

  XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
  spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  if (!ok(xrCreateReferenceSpace(g.session, &spaceInfo, &g.localSpace), "LOCAL-Space")) return false;
  spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  if (!ok(xrCreateReferenceSpace(g.session, &spaceInfo, &g.viewSpace), "VIEW-Space")) return false;

  if (!createActions()) return false;
  for (int h = 0; h < 2; ++h) {
    XrActionSpaceCreateInfo info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    info.action = g.aimPose;
    info.subactionPath = g.hand[h];
    info.poseInActionSpace.orientation.w = 1.0f;
    if (!ok(xrCreateActionSpace(g.session, &info, &g.aimSpace[h]), "Aim-Space")) return false;
  }
  XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach.countActionSets = 1;
  attach.actionSets = &g.actionSet;
  if (!ok(xrAttachSessionActionSets(g.session, &attach), "xrAttachSessionActionSets")) return false;

  chooseColorFormat();
  if (!createSwapchain(g.game, s.frameW, s.frameH, "Spiel")) return false;

  // Lese-FBOs für die geteilten Frame-Texturen (FBOs sind je Kontext).
  for (FrameSlot& slot : s.slots) {
    glGenFramebuffers(1, &slot.xrFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, slot.xrFbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, slot.texture, 0);
  }
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

  g.loaderReady = createSwapchain(g.loader, LoaderPanel::kWidth, LoaderPanel::kHeight, "Ladeanzeige");

  // Strahl-Layer (optional: ohne ihn bleibt das Spiel bedienbar, nur ohne sichtbaren Strahl).
  uint32_t viewCount = 0;
  xrEnumerateViewConfigurationViews(g.instance, g.systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                                    &viewCount, nullptr);
  std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  xrEnumerateViewConfigurationViews(g.instance, g.systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                    viewCount, &viewCount, views.data());
  if (viewCount == 2) {
    int ew = static_cast<int>(static_cast<float>(views[0].recommendedImageRectWidth) * kEyeScale);
    int eh = static_cast<int>(static_cast<float>(views[0].recommendedImageRectHeight) * kEyeScale);
    XLOGI("xr", "empfohlene Augenauflösung %ux%u, Strahl-Layer %dx%d", views[0].recommendedImageRectWidth,
          views[0].recommendedImageRectHeight, ew, eh);
    g.beamReady = createSwapchain(g.eyes[0], ew, eh, "Auge links") &&
                  createSwapchain(g.eyes[1], ew, eh, "Auge rechts") && g.beam.init();
  }
  if (!g.beamReady) XLOGW("xr", "Strahl-Layer nicht verfügbar");

  g.menuReady = createSwapchain(g.menu, VrMenu::kWidth, VrMenu::kHeight, "Menü");
  if (!g.menuReady) XLOGW("xr", "VR-Menü nicht verfügbar");
  g.keyboardReady = createSwapchain(g.keyboard, KeyboardPanel::kWidth, KeyboardPanel::kHeight, "Tastatur");
  if (!g.keyboardReady) XLOGW("xr", "VR-Tastatur nicht verfügbar");
  g.filterReady = g.filter.init();
  if (!g.filterReady) XLOGW("xr", "Bildfilter nicht verfügbar – Regler ohne Wirkung");

  if (g.passthroughAvailable) {
    auto load = [](const char* name, auto* fn) {
      return XR_SUCCEEDED(xrGetInstanceProcAddr(g.instance, name, reinterpret_cast<PFN_xrVoidFunction*>(fn))) &&
             *fn != nullptr;
    };
    bool loaded = load("xrCreatePassthroughFB", &g.xrCreatePassthroughFB_) &&
                  load("xrPassthroughStartFB", &g.xrPassthroughStartFB_) &&
                  load("xrPassthroughPauseFB", &g.xrPassthroughPauseFB_) &&
                  load("xrCreatePassthroughLayerFB", &g.xrCreatePassthroughLayerFB_) &&
                  load("xrPassthroughLayerResumeFB", &g.xrPassthroughLayerResumeFB_) &&
                  load("xrPassthroughLayerPauseFB", &g.xrPassthroughLayerPauseFB_);
    XrPassthroughCreateInfoFB ptInfo{XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
    XrPassthroughLayerCreateInfoFB layerInfo{XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
    g.passthroughAvailable =
        loaded && ok(g.xrCreatePassthroughFB_(g.session, &ptInfo, &g.passthrough), "xrCreatePassthroughFB");
    if (g.passthroughAvailable) {
      layerInfo.passthrough = g.passthrough;
      layerInfo.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
      g.passthroughAvailable =
          ok(g.xrCreatePassthroughLayerFB_(g.session, &layerInfo, &g.passthroughLayer), "xrCreatePassthroughLayerFB");
    }
    // Ohne IS_RUNNING_AT_CREATION starten beide pausiert; gestartet wird nach Einstellung.
    XLOGI("passthrough", "%s", g.passthroughAvailable ? "bereit" : "nicht verfügbar");
  }
  applyPassthrough();

  XLOGI("xr", "bereit: Spielbild %dx%d", s.frameW, s.frameH);
  return true;
}

// ================================================================ Laufzeit (XR-Thread)

void updateProfile() {
  std::string text;
  for (int h = 0; h < 2; ++h) {
    XrInteractionProfileState state{XR_TYPE_INTERACTION_PROFILE_STATE};
    if (XR_SUCCEEDED(xrGetCurrentInteractionProfile(g.session, g.hand[h], &state)) &&
        state.interactionProfile != XR_NULL_PATH) {
      char buf[XR_MAX_PATH_LENGTH];
      uint32_t len = 0;
      xrPathToString(g.instance, state.interactionProfile, sizeof(buf), &len, buf);
      text += (h == kLeft ? "links " : " rechts ");
      text += buf;
    }
  }
  g.profile = text.empty() ? "(keins)" : text;
  XLOGI("xr", "Eingabeprofil: %s", g.profile.c_str());
}

void pollEvents() {
  XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
  while (xrPollEvent(g.instance, &event) == XR_SUCCESS) {
    if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
      auto* changed = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
      XrSessionState previous = g.sessionState;
      g.sessionState = changed->state;
      XLOGI("xr", "Session %s -> %s", stateName(previous), stateName(g.sessionState));
      if (previous == XR_SESSION_STATE_FOCUSED) releaseAll("Fokus verloren");
      switch (g.sessionState) {
        case XR_SESSION_STATE_READY: {
          XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
          begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
          g.running = ok(xrBeginSession(g.session, &begin), "xrBeginSession");
          g.centered = false;  // Start immer zentriert vor der Blickrichtung
          break;
        }
        case XR_SESSION_STATE_STOPPING:
          xrEndSession(g.session);
          g.running = false;
          break;
        case XR_SESSION_STATE_EXITING:
        case XR_SESSION_STATE_LOSS_PENDING: {
          g.running = false;
          saveSettings(g.settings);
          SDL_Event quit;
          SDL_zero(quit);
          quit.type = SDL_QUIT;
          SDL_PushEvent(&quit);
          break;
        }
        default:
          break;
      }
    } else if (event.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
      updateProfile();
    } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
      XLOGE("xr", "OpenXR-Instanz geht verloren");
    }
    event = {XR_TYPE_EVENT_DATA_BUFFER};
  }
}

bool actionBool(XrAction action, XrPath subaction = XR_NULL_PATH) {
  XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
  info.action = action;
  info.subactionPath = subaction;
  XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
  return XR_SUCCEEDED(xrGetActionStateBoolean(g.session, &info, &state)) && state.isActive &&
         state.currentState;
}

float actionFloat(XrAction action, int hand) {
  XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
  info.action = action;
  info.subactionPath = g.hand[hand];
  XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
  if (XR_SUCCEEDED(xrGetActionStateFloat(g.session, &info, &state)) && state.isActive) {
    return state.currentState;
  }
  return 0.0f;
}

XrVector2f actionStick(int hand) {
  XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
  info.action = g.stick;
  info.subactionPath = g.hand[hand];
  XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
  if (XR_SUCCEEDED(xrGetActionStateVector2f(g.session, &info, &state)) && state.isActive) {
    return state.currentState;
  }
  return {0.0f, 0.0f};
}

// Menü wie in vielen VR-Spielen an der Hand: über dem Controller der Nicht-Zeigerhand, zum
// Kopf gedreht (Yaw und Neigung). Ohne gültiges Handtracking bleibt die letzte Lage; beim
// Öffnen ohne Hand steht es vor dem Kopf.
bool updateMenuPose(XrTime time) {
  if (!g.headValid) return false;
  XrSpaceLocation hand{XR_TYPE_SPACE_LOCATION};
  xrLocateSpace(g.aimSpace[otherHand()], g.localSpace, time, &hand);
  const XrSpaceLocationFlags valid =
      XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  if ((hand.locationFlags & valid) != valid) return false;
  XrVector3f forward = rotate(hand.pose.orientation, {0.0f, 0.0f, -1.0f});
  forward.y = 0.0f;
  forward = normalize(forward);
  XrVector3f pos = add(hand.pose.position, scale(forward, kMenuForward));
  pos.y += kMenuAboveHand;
  XrVector3f toHead = normalize(sub(g.head.position, pos));
  float yaw = std::atan2(toHead.x, toHead.z);
  float pitch = -std::asin(std::clamp(toHead.y, -1.0f, 1.0f));
  g.menuPose = {multiply(yawQuat(yaw), pitchQuat(pitch)), pos};
  return true;
}

void openMenu(XrTime time) {
  if (!g.menuReady || !g.headValid) return;
  releaseAll("Menü geöffnet");
  if (!updateMenuPose(time)) {
    float yaw = yawOf(g.head.orientation);
    XrQuaternionf q = yawQuat(yaw);
    XrVector3f pos = add(g.head.position, scale(rotate(q, {0.0f, 0.0f, -1.0f}), kMenuDistance));
    pos.y -= kMenuBelowEyes;
    g.menuPose = {q, pos};
  }
  g.menuOpen = true;
  g.menuNote.clear();
  // P5: immer im Schnellmenü öffnen; Stick-Fokus startet auf „Weiterspielen“.
  g.vrMenu.setPage(kPageQuick);
  g.vrMenu.setHover(-1);
  g.menuRayHover = -1;
  g.menuStickFocus = false;
  g.menuStick.reset();
  g.menuTrigger.reset();
  g.menuA.reset();
  XLOGI("menu", "geöffnet");
}

// P3 Beenden: erst speichern, dann sauber beenden. SDL_QUIT allein reicht bei Stratagus nicht (in einer
// Partie führt es nur ins Hauptmenü); daher Activity.finishAndRemoveTask() → onDestroy →
// SDL_APP_TERMINATING → android_main beendet sofort. Rückfall: SDL_QUIT.
void requestQuit() {
  if (g.quitting) return;
  g.quitting = true;
  saveSettings(g.settings);
  XLOGI("menu", "Beenden bestätigt – Einstellungen gespeichert, App wird geschlossen");
  JNIEnv* env = nullptr;
  bool attached = false;
  if (gt.vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    attached = gt.vm->AttachCurrentThread(&env, nullptr) == JNI_OK;
    if (!attached) env = nullptr;
  }
  bool done = false;
  if (env && gt.activity) {
    jclass cls = env->GetObjectClass(gt.activity);
    jmethodID finish = cls ? env->GetMethodID(cls, "finishAndRemoveTask", "()V") : nullptr;
    if (finish) {
      env->CallVoidMethod(gt.activity, finish);
      done = !env->ExceptionCheck();
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
  }
  if (attached) gt.vm->DetachCurrentThread();
  if (!done) {
    XLOGW("menu", "finishAndRemoveTask nicht möglich – SDL_QUIT");
    SDL_Event quit;
    SDL_zero(quit);
    quit.type = SDL_QUIT;
    SDL_PushEvent(&quit);
  }
}

void closeMenu() {
  if (!g.menuOpen) return;
  g.menuOpen = false;
  g.quitConfirm.cancel();  // nie scharf in einen neuen Kontext mitnehmen
  g.shiftNeedsRelease = true;  // A vom Menü schlägt nicht als Umschalt ins Spiel durch
  releaseAll("Menü geschlossen");
  saveSettings(g.settings);
  XLOGI("menu", "geschlossen");
}

void openKeyboard() {
  if (!g.keyboardReady) return;
  float yaw = g.headValid ? yawOf(g.head.orientation) : g.yaw;
  XrQuaternionf q = multiply(yawQuat(yaw), pitchQuat(kKeyboardPitch));
  XrVector3f base = g.headValid ? g.head.position : g.anchor;
  XrVector3f pos = add(base, scale(rotate(yawQuat(yaw), {0.0f, 0.0f, -1.0f}), kKeyboardDistance));
  pos.y -= kKeyboardBelowEyes;
  g.keyboardPose = {q, pos};
  g.keyboardPanel.setShift(false);
  g.keyboardPanel.setHover(-1);
  g.keyClick.reset();  // der Klick, der die Tastatur geöffnet hat, zählt nicht als Taste
  g.keyboardOpen = true;
  XLOGI("keyboard", "geöffnet");
}

void closeKeyboard(const char* reason) {
  if (!g.keyboardOpen) return;
  g.keyboardOpen = false;
  g.leftClick.reset();
  g.rightClick.reset();
  XLOGI("keyboard", "geschlossen (%s)", reason);
}

// Passthrough an/aus (XR_FB_passthrough). Ohne Unterstützung bleibt der Hintergrund schwarz.
void applyPassthrough() {
  bool want = g.settings.passthrough && g.passthroughAvailable;
  if (want == g.passthroughRunning) return;
  if (want) {
    bool started = ok(g.xrPassthroughStartFB_(g.passthrough), "xrPassthroughStartFB") &&
                   ok(g.xrPassthroughLayerResumeFB_(g.passthroughLayer), "xrPassthroughLayerResumeFB");
    g.passthroughRunning = started;
    XLOGI("passthrough", "%s", started ? "an" : "Start fehlgeschlagen");
  } else {
    g.xrPassthroughLayerPauseFB_(g.passthroughLayer);
    g.xrPassthroughPauseFB_(g.passthrough);
    g.passthroughRunning = false;
    XLOGI("passthrough", "aus");
  }
}

void skipMovie(const char* why) {
  if (!s.movieRunning.load()) return;
  // movie.cpp beendet den Film bei jedem Tastendruck.
  tapKey(SDLK_ESCAPE);
  XLOGI("movie", "Film übersprungen (%s)", why);
}

void selectGroup(int group) {
  SDL_Keycode key = static_cast<SDL_Keycode>('0' + group);
  switch (g.groupMode) {
    case GroupMode::kSelect: tapChar(static_cast<char>('0' + group)); break;       // wählen
    case GroupMode::kStore: tapWithModifiers(key, true, false); break;              // Strg+Zahl
    case GroupMode::kAdd: tapWithModifiers(key, true, true); break;                 // Strg+Umschalt+Zahl
  }
}

void runMenuAction(const MenuHit& hit) {
  Settings& st = g.settings;
  bool closeAfter = false;
  auto slider = [](int& v, int delta) { v = std::clamp(v + delta, 0, 100); };
  // Jeder andere Klick (auch Reiterwechsel) entschärft „Beenden“.
  if (hit.action != MenuAction::kQuit) g.quitConfirm.cancel();
  switch (hit.action) {
    case MenuAction::kOpenSettings: g.vrMenu.setPage(kPageGame); break;
    case MenuAction::kBack: g.vrMenu.setPage(kPageQuick); break;
    case MenuAction::kClose: closeAfter = true; break;
    case MenuAction::kTabGame: g.vrMenu.setPage(kPageGame); break;
    case MenuAction::kTabWindow: g.vrMenu.setPage(kPageWindow); break;
    case MenuAction::kTabDisplay: g.vrMenu.setPage(kPageDisplay); break;
    case MenuAction::kQuit:
      if (g.quitConfirm.click() == ConfirmAction::kArmed) {
        g.secondPulse = kSecondPulseDelay;  // Doppelpuls: zweiter Impuls folgt
        XLOGI("menu", "Beenden scharf – zweiter Klick innerhalb 3 s beendet");
      } else {
        requestQuit();
      }
      break;
    case MenuAction::kToggleGameCursor:
      st.gameCursor = !st.gameCursor;
      s.gameCursor = st.gameCursor;
      break;
    case MenuAction::kSaturation: slider(st.image.saturation, hit.value); break;
    // Spiel
    case MenuAction::kResume: closeAfter = true; break;
    case MenuAction::kGameMenu: tapKey(SDLK_F10); closeAfter = true; break;
    case MenuAction::kPause: tapKey(SDLK_PAUSE); break;
    case MenuAction::kHelp: tapKey(SDLK_F1); closeAfter = true; break;
    case MenuAction::kSave: tapKey(SDLK_F11); closeAfter = true; break;
    case MenuAction::kLoad: tapKey(SDLK_F12); closeAfter = true; break;
    case MenuAction::kSpeedUp: tapChar('='); g.menuNote = "schneller"; break;
    case MenuAction::kSpeedDown: tapChar('-'); g.menuNote = "langsamer"; break;
    case MenuAction::kIdleWorker: tapChar('.'); closeAfter = true; break;
    case MenuAction::kLastMessage: tapChar(' '); closeAfter = true; break;
    case MenuAction::kCenterSelection: tapChar('c'); closeAfter = true; break;
    case MenuAction::kKeyboard: openKeyboard(); closeAfter = true; break;
    case MenuAction::kSkipMovie: skipMovie("Menü"); closeAfter = true; break;
    case MenuAction::kToggleAutoSkip:
      st.autoSkipMovies = !st.autoSkipMovies;
      if (st.autoSkipMovies) skipMovie("automatisch");
      break;
    // Gruppen
    case MenuAction::kGroupMode: g.groupMode = static_cast<GroupMode>(std::clamp(hit.value, 0, 2)); break;
    case MenuAction::kGroup:
      selectGroup(hit.value);
      // Nach Speichern/Ergänzen zurück auf „Wählen“: der nächste Griff zur Zahl überschreibt nichts.
      g.groupMode = GroupMode::kSelect;
      closeAfter = true;
      break;
    case MenuAction::kToggleInputLog:
      st.logInput = !st.logInput;
      setInputLogging(st.logInput);
      break;
    case MenuAction::kDiagnose: {
      logDiagnose();
      char note[48];
      std::snprintf(note, sizeof(note), "Diagnose #%d im Log", g.diagnoseCount);
      g.menuNote = note;
      break;
    }
    // Fenster & Bild
    case MenuAction::kRecenter: g.centered = false; break;
    case MenuAction::kResetWindow:
      st.width = kDefaultWidth;
      st.distance = kDefaultDistance;
      g.centered = false;
      break;
    case MenuAction::kSmaller: st.width -= kMenuStep; clampPanel(); break;
    case MenuAction::kLarger: st.width += kMenuStep; clampPanel(); break;
    case MenuAction::kCloser: st.distance -= kMenuStep; clampPanel(); break;
    case MenuAction::kFarther: st.distance += kMenuStep; clampPanel(); break;
    case MenuAction::kToggleHand:
      st.pointerHand = st.pointerHand ? 0 : 1;
      releaseAll("Zeigerhand gewechselt");
      break;
    case MenuAction::kToggleBeam: st.beam = !st.beam; break;
    case MenuAction::kCycleTarget:
      st.target = static_cast<TargetStyle>((static_cast<int>(st.target) + 1) % 3);
      break;
    case MenuAction::kTogglePassthrough:
      if (g.passthroughAvailable) {
        st.passthrough = !st.passthrough;
        applyPassthrough();
      }
      break;
    case MenuAction::kToggleMenuStyle:
      st.menuOrc = !st.menuOrc;
      Theme::get().setOrc(st.menuOrc);
      g.vrMenu.invalidate();
      g.keyboardPanel.setHover(-1);
      break;
    case MenuAction::kImageReset: st.image = ImageSettings{}; break;
    case MenuAction::kBrightness: slider(st.image.brightness, hit.value); break;
    case MenuAction::kContrast: slider(st.image.contrast, hit.value); break;
    case MenuAction::kSharpness: slider(st.image.sharpness, hit.value); break;
    case MenuAction::kCycleCrt: st.image.crt = (st.image.crt + 1) % 3; break;
  }
  saveSettings(st);
  if (closeAfter) closeMenu();
}

// Eingaben bei offenem Menü (P2): Strahl + Trigger oder Stick + A, B/Menütaste schließen. Der Fokus
// folgt dem Strahl, sobald dieser eine andere Schaltfläche trifft, sonst dem Stick (auch neben dem Menü).
void processMenuInput(const XrPosef& aim, bool tracked, float dt) {
  int rayHover = -1;
  float u = 0.0f, v = 0.0f;
  XrVector3f point{};
  if (tracked && hitQuad(aim, g.menuPose, kMenuWidth, menuHeight(), &u, &v, &point) && u >= 0.0f &&
      u <= 1.0f && v >= 0.0f && v <= 1.0f) {
    rayHover = g.vrMenu.hit(u, v);
    g.beamEnd = point;
    g.beamHit = true;
  }
  int hover = g.vrMenu.hover();
  if (rayHover != g.menuRayHover) {
    if (rayHover >= 0 || !g.menuStickFocus) hover = rayHover;
    if (rayHover >= 0) g.menuStickFocus = false;
    g.menuRayHover = rayHover;
  }

  int dx = 0, dy = 0;
  XrVector2f st = actionStick(pointerHand());
  stickDirection(st.x, st.y, &dx, &dy);
  if (dx != g.menuStickDx || dy != g.menuStickDy) {
    g.menuStick.update(false, dt);  // Richtungswechsel = neue Flanke
    g.menuStickDx = dx;
    g.menuStickDy = dy;
  }
  for (int n = g.menuStick.update(dx != 0 || dy != 0, dt); n > 0; --n) {
    hover = g.vrMenu.navigate(g.menuStickFocus ? hover : -1, dx, dy);
    g.menuStickFocus = true;
  }
  g.vrMenu.setHover(hover);

  // Auslösen: Flanke immer, Dauerdruck nur bei ‹ ›-Aktionen (Größe, Abstand, Bildregler, Tempo).
  auto activate = [&](int index, int fires) {
    for (int n = 0; n < fires && index >= 0 && index < g.vrMenu.count(); ++n) {
      MenuHit hit = g.vrMenu.action(index);
      if (g.vrMenu.grayed(index) || (n > 0 && !menuActionRepeats(hit.action))) break;
      pulse();
      XLOGI("menu", "Aktion: %s", g.vrMenu.label(index).c_str());
      int page = g.vrMenu.page();
      runMenuAction(hit);
      if (!g.menuOpen || g.vrMenu.page() != page) break;
    }
  };
  g.menuClick.update(actionFloat(g.trigger, pointerHand()));
  int triggerFires = g.menuTrigger.update(g.menuClick.down(), dt);
  if (triggerFires > 0 && rayHover >= 0) {
    g.vrMenu.setHover(rayHover);
    activate(rayHover, triggerFires);
  }
  if (!g.menuOpen) return;
  int aFires = g.menuA.update(actionBool(g.buttonA), dt);
  if (aFires > 0) {
    int focus = g.vrMenu.hover();
    if (focus < 0) {
      // A ohne Fokus: erst Fokus setzen (sichtbar), nicht blind auslösen.
      g.vrMenu.setHover(g.vrMenu.navigate(-1, 0, 0));
      g.menuStickFocus = true;
    } else {
      activate(focus, aFires);
    }
  }
  if (!g.menuOpen) return;
  if (g.pressB.update(actionBool(g.buttonB), dt) == PressButton::kShort ||
      g.pressMenu.update(actionBool(g.menuButton), dt) == PressButton::kShort) {
    closeMenu();
  }
}

float keyboardHeight() {
  return kKeyboardWidth * static_cast<float>(KeyboardPanel::kHeight) / static_cast<float>(KeyboardPanel::kWidth);
}

// Strahl auf der Tastatur: Tasten antippen. Liefert true, wenn der Strahl sie trifft (dann
// gehen Trigger/Griff dieses Frames nicht ans Spiel).
bool processKeyboardInput(const XrPosef& aim, bool tracked) {
  float u = 0.0f, v = 0.0f;
  XrVector3f point{};
  if (!tracked || !hitQuad(aim, g.keyboardPose, kKeyboardWidth, keyboardHeight(), &u, &v, &point) || u < 0.0f ||
      u > 1.0f || v < 0.0f || v > 1.0f) {
    g.keyboardPanel.setHover(-1);
    g.keyClick.update(0.0f);
    return false;
  }
  g.beamEnd = point;
  g.beamHit = true;
  int index = g.keyboardPanel.hit(u, v);
  g.keyboardPanel.setHover(index);
  if (g.keyClick.update(actionFloat(g.trigger, pointerHand())) > 0 && index >= 0) {
    pulse();
    KeyHit key = g.keyboardPanel.press(index);
    switch (key.kind) {
      case KeyKind::kText: typeText(key.text); break;
      case KeyKind::kBackspace: tapKeyQuiet(SDLK_BACKSPACE); break;
      case KeyKind::kSuggest: typeSuggestion(); break;
      case KeyKind::kCancel: tapKeyQuiet(SDLK_ESCAPE); closeKeyboard("Abbrechen"); break;
      case KeyKind::kConfirm: tapKeyQuiet(SDLK_RETURN); closeKeyboard("Übernehmen"); break;
      case KeyKind::kShift:
      case KeyKind::kSpace: break;
    }
  }
  return true;
}

void processGameInput(const XrPosef& aim, bool tracked, float dt) {
  const int ph = pointerHand();
  const int oh = otherHand();

  // --- Zeiger
  float u = 0.0f, v = 0.0f;
  XrVector3f point{};
  bool onPanel = false;
  ++g.pointerFrames;
  if (tracked && hitQuad(aim, panelPose(), g.settings.width, panelHeight(), &u, &v, &point)) {
    bool inside = u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f;
    bool dragging = g.leftClick.down() || g.rightClick.down();
    onPanel = inside;
    if (inside) {
      ++g.pointerHits;
      g.beamEnd = point;
      g.beamHit = true;
    }
    if (inside || dragging) {
      if (g.freezeLeft > 0.0f) {
        g.freezeLeft -= dt;  // Klickstabilisierung: Zielpunkt kurz einfrieren
      } else {
        float a = g.pointerValid ? smoothingAlpha(dt, kPointerTau) : 1.0f;
        float newU = g.pointerU + (u - g.pointerU) * a;
        float newV = g.pointerV + (v - g.pointerV) * a;
        bool moved = !g.pointerValid || std::fabs(newU - g.pointerU) > 1e-5f ||
                     std::fabs(newV - g.pointerV) > 1e-5f;
        g.pointerU = newU;
        g.pointerV = newV;
        g.pointerValid = true;
        if (moved) requestWarp();
      }
    }
  }

  // --- Klicks (Zeigerhand): Trigger = links (auswählen, Rahmen ziehen), Griff = rechts (Befehl)
  int trig = g.leftClick.update(actionFloat(g.trigger, ph));
  if (trig > 0) {
    if (onPanel) {
      g.freezeLeft = kClickFreeze;
      sendMouseButton(SDL_BUTTON_LEFT, true);
      pulse();
    } else {
      g.leftClick.reset();  // neben dem Fenster: kein Klick, erst loslassen
      if (inputLogging()) XLOGD("input", "Trigger neben dem Fenster ignoriert");
    }
  } else if (trig < 0) {
    sendMouseButton(SDL_BUTTON_LEFT, false);
  }
  int grip = g.rightClick.update(actionFloat(g.grip, ph));
  if (grip > 0) {
    if (onPanel) {
      sendMouseButton(SDL_BUTTON_RIGHT, true);  // Down sofort, Up erst beim Loslassen
      pulse();
    } else {
      g.rightClick.reset();
    }
  } else if (grip < 0) {
    sendMouseButton(SDL_BUTTON_RIGHT, false);
  }

  // --- Stick der Zeigerhand: Karte scrollen (Pfeiltasten, Kanten-Scrollen ist aus);
  //     Stick drücken = Auswahl zentrieren.
  XrVector2f rs = actionStick(ph);
  float sx = applyDeadzone(rs.x);
  float sy = applyDeadzone(rs.y);
  setHeld(g.keys.right, sx > 0.0f, SDLK_RIGHT);
  setHeld(g.keys.left, sx < 0.0f, SDLK_LEFT);
  setHeld(g.keys.up, sy > 0.0f, SDLK_UP);
  setHeld(g.keys.down, sy < 0.0f, SDLK_DOWN);
  if (g.pressStick.update(actionBool(g.stickClick, g.hand[ph]), dt) == PressButton::kShort) tapChar('c');

  // --- Griff der anderen Hand halten: Fenster mit deren Stick verstellen (sonst gesperrt)
  g.adjustGrip.update(actionFloat(g.grip, oh));
  bool adjustStick = actionBool(g.stickClick, g.hand[oh]);
  if (g.adjustGrip.down()) {
    XrVector2f ls = actionStick(oh);
    float lx = applyDeadzone(ls.x);
    float ly = applyDeadzone(ls.y);
    if (lx != 0.0f || ly != 0.0f) {
      g.settings.width += lx * kAdjustSpeed * dt;
      g.settings.distance -= ly * kAdjustSpeed * dt;
      clampPanel();
      g.adjusting = true;
      saveSoon();
    } else if (g.adjusting) {
      g.adjusting = false;
      XLOGI("window", "verstellt: Breite %.2f m, Abstand %.2f m", g.settings.width, g.settings.distance);
    }
    if (g.pressAdjustStick.update(adjustStick, dt) == PressButton::kShort) {
      g.settings.width = kDefaultWidth;
      g.settings.distance = kDefaultDistance;
      g.centered = false;
      saveSoon();
      XLOGI("window", "zurückgesetzt");
    }
  } else {
    g.pressAdjustStick.update(false, dt);
    g.adjusting = false;
  }

  // --- Tasten
  if (g.pressMenu.update(actionBool(g.menuButton), dt) == PressButton::kShort) {
    openMenu(g.frameTime);
    return;
  }
  // Modifikatoren halten: A = Umschalt (Auswahl ergänzen), X = Strg (Gruppen, alle gleichen Typs).
  // Bei offener VR-Tastatur nicht (nie Zeichen/Aktionen im Textfeld auslösen).
  const bool typing = g.keyboardOpen;
  bool aDown = actionBool(g.buttonA);
  if (!aDown) g.shiftNeedsRelease = false;
  setHeld(g.keys.shift, aDown && !typing && !g.shiftNeedsRelease, SDLK_LSHIFT);
  setHeld(g.keys.ctrl, actionBool(g.buttonX) && !typing, SDLK_LCTRL);
  // Y: kurz Pause, lang Spielmenü (F10).
  switch (g.pressY.update(actionBool(g.buttonY) && !typing, dt)) {
    case PressButton::kShort: tapKey(SDLK_PAUSE); break;
    case PressButton::kLong: tapKey(SDLK_F10); break;
    default: break;
  }
  // B: Escape (Befehl abbrechen, Dialog schließen, Film überspringen).
  if (g.pressB.update(actionBool(g.buttonB), dt) == PressButton::kShort) {
    if (loaderVisible() && (g.load.error || g.load.dataMissing)) {
      g.loaderDismissed = true;  // Fehleranzeige weg, Spielbild zeigen
      XLOGI("loader", "Fehleranzeige per B geschlossen");
    } else if (g.keyboardOpen) {
      closeKeyboard("B");
    } else {
      tapKey(SDLK_ESCAPE);
    }
  }
}

void processInput(XrTime time, float dt) {
  g.frameTime = time;
  for (SDL_Keycode k : g.pendingKeyUps) sendKey(k, false);
  g.pendingKeyUps.clear();
  g.beamVisible = false;
  g.beamHit = false;

  // Beenden-Bestätigung läuft nach 3 s ab; Doppelpuls beim Scharfschalten.
  g.quitConfirm.update(dt);
  if (g.secondPulse >= 0.0f) {
    g.secondPulse -= dt;
    if (g.secondPulse < 0.0f) pulse();
  }

  // Filme automatisch überspringen (Einstellung). Nicht schon beim Anlegen der Filmtextur: dann
  // startet Stratagus gerade erst die Filmmusik und hat die Film-Tastenbehandlung noch nicht gesetzt.
  // Erst nach kMovieSkipDelay laufendem Film, bei Bedarf bis zu dreimal im Abstand von 1 s.
  uint32_t movies = s.moviesStarted.load();
  if (movies != g.moviesSeen) {
    g.moviesSeen = movies;
    g.movieSkipIn = g.settings.autoSkipMovies ? kMovieSkipDelay : -1.0f;
    g.movieSkipTries = 0;
  }
  if (!s.movieRunning.load()) {
    g.movieSkipIn = -1.0f;
  } else if (g.movieSkipIn >= 0.0f) {
    g.movieSkipIn -= dt;
    if (g.movieSkipIn < 0.0f && g.movieSkipTries < 3) {
      ++g.movieSkipTries;
      skipMovie("automatisch");
      g.movieSkipIn = 1.0f;
    }
  }

  if (g.sessionState != XR_SESSION_STATE_FOCUSED) return;

  XrActiveActionSet active{g.actionSet, XR_NULL_PATH};
  XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
  sync.countActiveActionSets = 1;
  sync.activeActionSets = &active;
  if (!XR_SUCCEEDED(xrSyncActions(g.session, &sync))) return;

  XrSpaceLocation aim{XR_TYPE_SPACE_LOCATION};
  xrLocateSpace(g.aimSpace[pointerHand()], g.localSpace, time, &aim);
  const XrSpaceLocationFlags validPose =
      XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  bool tracked = (aim.locationFlags & validPose) == validPose;
  if (!tracked && (g.leftClick.down() || g.rightClick.down())) {
    releaseAll("Controller-Tracking verloren");
  }
  if (tracked) {
    XrVector3f dir = rotate(aim.pose.orientation, {0.0f, 0.0f, -1.0f});
    g.beamStart = add(aim.pose.position, scale(dir, 0.03f));
    g.beamEnd = add(aim.pose.position, scale(dir, kBeamLength));
    g.beamVisible = true;
  }

  if (g.menuOpen) {
    updateMenuPose(time);  // Menü folgt der Hand
    processMenuInput(aim.pose, tracked, dt);
  } else if (g.keyboardOpen && !g.leftClick.down() && !g.rightClick.down() &&
             processKeyboardInput(aim.pose, tracked)) {
    // Strahl auf der Tastatur: Tasten dort, Spiel ruht. Re-arm, damit ein gehaltener Trigger
    // beim Wechsel aufs Spielfenster nicht als neuer Klick zählt; Scrolltasten lösen.
    g.leftClick.reset();
    g.rightClick.reset();
    releaseScroll();
    // Menütaste bleibt erreichbar, B schließt die Tastatur.
    if (g.pressMenu.update(actionBool(g.menuButton), dt) == PressButton::kShort) openMenu(time);
    if (g.pressB.update(actionBool(g.buttonB), dt) == PressButton::kShort) closeKeyboard("B");
  } else {
    processGameInput(aim.pose, tracked, dt);
  }

  if (g.saveTimer >= 0.0f) {
    g.saveTimer -= dt;
    if (g.saveTimer < 0.0f) saveSettings(g.settings);
  }
}

// ================================================================ Zeichnen (XR-Thread)

// Neuestes Spielbild aus dem geteilten Platz in das Swapchain-Bild kopieren.
bool copyGameFrame(GLuint drawFbo) {
  int index;
  GLsync written;
  {
    std::lock_guard<std::mutex> lock(s.m);
    index = s.latest;
    if (index < 0) return false;
    s.reading = index;
    written = s.slots[index].written;
    s.slots[index].written = nullptr;
    s.consumed = s.published;
  }
  s.cv.notify_all();
  if (written) {
    glWaitSync(written, 0, GL_TIMEOUT_IGNORED);
    glDeleteSync(written);
  }
  const ImageSettings& img = g.settings.image;
  if (g.filterReady && !img.neutral()) {
    // Bildregler aktiv: mit Filter-Shader zeichnen.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
    glViewport(0, 0, g.game.width, g.game.height);
    glDisable(GL_BLEND);
    g.filter.draw(s.slots[index].texture, s.frameW, s.frameH, img.brightness, img.contrast, img.saturation,
                  img.sharpness, img.crt, g.game.width, g.game.height);
  } else {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s.slots[index].xrFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
    glBlitFramebuffer(0, 0, s.frameW, s.frameH, 0, 0, g.game.width, g.game.height, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
  }
  GLsync readDone = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();
  {
    std::lock_guard<std::mutex> lock(s.m);
    if (s.slots[index].readDone) glDeleteSync(s.slots[index].readDone);
    s.slots[index].readDone = readDone;
    s.reading = -1;
  }
  return true;
}

// CPU-Bild (Zeile 0 oben) in das Swapchain-Bild laden, falls dort noch ein älterer Stand liegt.
void uploadCanvas(Swapchain& sc, uint32_t index, const uint8_t* src, uint32_t version) {
  if (sc.uploadedVersion[index] == version) return;
  // GL-Texturen beginnen unten: Zeilen umdrehen.
  static std::vector<uint8_t> flipped;
  const size_t row = static_cast<size_t>(sc.width) * 4;
  flipped.resize(row * static_cast<size_t>(sc.height));
  for (int y = 0; y < sc.height; ++y) {
    std::memcpy(&flipped[static_cast<size_t>(sc.height - 1 - y) * row], src + static_cast<size_t>(y) * row, row);
  }
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glBindTexture(GL_TEXTURE_2D, sc.images[index].image);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sc.width, sc.height, GL_RGBA, GL_UNSIGNED_BYTE, flipped.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  sc.uploadedVersion[index] = version;
}

void quadVertices(float* out, XrVector3f a, XrVector3f b, XrVector3f c, XrVector3f d) {
  const XrVector3f pts[4] = {a, b, c, d};
  const float uv[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
  for (int i = 0; i < 4; ++i) {
    out[i * 5 + 0] = pts[i].x;
    out[i * 5 + 1] = pts[i].y;
    out[i * 5 + 2] = pts[i].z;
    out[i * 5 + 3] = uv[i][0];
    out[i * 5 + 4] = uv[i][1];
  }
}

void drawBeamEye(const XrView& view, GLuint framebuffer, const Swapchain& sc) {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, sc.width, sc.height);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

  Mat4 mvp = multiply(projection(view.fov, 0.05f, 100.0f), viewMatrix(view.pose));
  XrVector3f eye = view.pose.position;
  float verts[20];

  const PointerVisibility vis = pointerVisibility();
  if (vis.showRay(overlayOpen())) {
    XrVector3f dir = normalize(sub(g.beamEnd, g.beamStart));
    XrVector3f side = scale(normalize(cross(dir, sub(eye, g.beamStart))), kBeamHalfWidth);
    quadVertices(verts, sub(g.beamStart, side), add(g.beamStart, side), sub(g.beamEnd, side),
                 add(g.beamEnd, side));
    const float color[4] = {1.0f, 0.9f, 0.45f, g.beamHit ? 0.6f : 0.35f};  // Wargus-Gold
    g.beam.draw(mvp.m, verts, color, BeamRenderer::kBeam);
  }
  if (g.beamHit && vis.showMarker(overlayOpen())) {
    XrVector3f right = rotate(view.pose.orientation, {1.0f, 0.0f, 0.0f});
    XrVector3f up = rotate(view.pose.orientation, {0.0f, 1.0f, 0.0f});
    float r = 0.01f * length(sub(g.beamEnd, eye));
    XrVector3f rx = scale(right, r), ry = scale(up, r);
    XrVector3f c = g.beamEnd;
    quadVertices(verts, sub(sub(c, rx), ry), add(sub(c, rx), ry), add(sub(c, ry), rx), add(add(c, rx), ry));
    const float color[4] = {1.0f, 1.0f, 1.0f, 0.9f};
    g.beam.draw(mvp.m, verts, color,
                g.settings.target == TargetStyle::kDot ? BeamRenderer::kDot : BeamRenderer::kRing);
  }
  glDisable(GL_BLEND);
}

bool acquire(Swapchain& sc, uint32_t* index) {
  XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  wait.timeout = XR_INFINITE_DURATION;
  return ok(xrAcquireSwapchainImage(sc.handle, &acquireInfo, index), "Acquire") &&
         ok(xrWaitSwapchainImage(sc.handle, &wait), "Wait");
}

void release(Swapchain& sc) {
  XrSwapchainImageReleaseInfo info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  xrReleaseSwapchainImage(sc.handle, &info);
}

void frame() {
  uint64_t waitStart = nowNs();
  XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frameState{XR_TYPE_FRAME_STATE};
  if (!ok(xrWaitFrame(g.session, &waitInfo, &frameState), "xrWaitFrame")) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return;
  }
  uint64_t workStart = nowNs();
  g.waitSum += static_cast<double>(workStart - waitStart) * 1e-6;
  XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
  if (!ok(xrBeginFrame(g.session, &beginInfo), "xrBeginFrame")) return;
  ++g.xrFrames;

  XrTime time = frameState.predictedDisplayTime;
  float dt = g.lastDisplayTime ? static_cast<float>(time - g.lastDisplayTime) * 1e-9f : 0.0f;
  dt = std::clamp(dt, 0.0f, 0.1f);
  g.lastDisplayTime = time;

  XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
  xrLocateSpace(g.viewSpace, g.localSpace, time, &head);
  const XrSpaceLocationFlags valid =
      XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  g.headValid = (head.locationFlags & valid) == valid;
  if (g.headValid) g.head = head.pose;
  // Sobald die Kopfpose gültig ist – nicht erst bei FOCUSED warten.
  if (!g.centered && g.headValid) recenter(head.pose);

  processInput(time, dt);
  updateLoader(dt);

  XrCompositionLayerQuad panelQuad{XR_TYPE_COMPOSITION_LAYER_QUAD};
  XrCompositionLayerQuad menuQuad{XR_TYPE_COMPOSITION_LAYER_QUAD};
  XrCompositionLayerQuad keyboardQuad{XR_TYPE_COMPOSITION_LAYER_QUAD};
  XrCompositionLayerPassthroughFB passthroughLayer{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
  XrCompositionLayerProjection beamLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  XrCompositionLayerProjectionView beamViews[2] = {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
                                                   {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
  const XrCompositionLayerBaseHeader* layers[5] = {};
  uint32_t layerCount = 0;

  // Passthrough als eigene Unterlage (Umgebung bleibt OPAQUE, der Raum wird explizit eingereicht).
  if (g.passthroughRunning) {
    passthroughLayer.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    passthroughLayer.layerHandle = g.passthroughLayer;
    layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&passthroughLayer);
  }

  if (frameState.shouldRender && g.centered) {
    uint32_t index = 0;
    panelQuad.space = g.localSpace;
    panelQuad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    panelQuad.pose = panelPose();
    bool showLoader = g.loaderReady && loaderVisible();
    if (showLoader) {
      if (acquire(g.loader, &index)) {
        const uint8_t* pixels = g.loaderPanel.pixels(g.load);
        uploadCanvas(g.loader, index, pixels, g.loaderPanel.version());
        release(g.loader);
        panelQuad.subImage.swapchain = g.loader.handle;
        panelQuad.subImage.imageRect = {{0, 0}, {g.loader.width, g.loader.height}};
        panelQuad.size = {g.settings.width, g.settings.width * static_cast<float>(LoaderPanel::kHeight) /
                                                static_cast<float>(LoaderPanel::kWidth)};
        layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&panelQuad);
      }
      // Spielbilder trotzdem abnehmen, damit die Engine nicht wartet.
      std::lock_guard<std::mutex> lock(s.m);
      if (s.latest >= 0) g.hasGameFrame = true;
      s.consumed = s.published;
      s.cv.notify_all();
    } else if (acquire(g.game, &index)) {
      g.hasGameFrame = copyGameFrame(g.game.framebuffers[index]) || g.hasGameFrame;
      release(g.game);
      panelQuad.subImage.swapchain = g.game.handle;
      panelQuad.subImage.imageRect = {{0, 0}, {g.game.width, g.game.height}};
      panelQuad.size = {g.settings.width, panelHeight()};
      layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&panelQuad);
    }

    if (g.keyboardOpen && !g.menuOpen) {
      if (acquire(g.keyboard, &index)) {
        const uint8_t* pixels = g.keyboardPanel.pixels();
        uploadCanvas(g.keyboard, index, pixels, g.keyboardPanel.version());
        release(g.keyboard);
        keyboardQuad.space = g.localSpace;
        keyboardQuad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        keyboardQuad.subImage.swapchain = g.keyboard.handle;
        keyboardQuad.subImage.imageRect = {{0, 0}, {g.keyboard.width, g.keyboard.height}};
        keyboardQuad.pose = g.keyboardPose;
        keyboardQuad.size = {kKeyboardWidth, keyboardHeight()};
        layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&keyboardQuad);
      }
    }

    if (g.menuOpen && g.menuReady) {
      g.vrMenu.update(g.settings, g.groupMode, statusLine(), g.passthroughAvailable, s.movieRunning.load(),
                     g.quitConfirm.armed());
      if (acquire(g.menu, &index)) {
        const uint8_t* pixels = g.vrMenu.pixels();
        uploadCanvas(g.menu, index, pixels, g.vrMenu.version());
        release(g.menu);
        menuQuad.space = g.localSpace;
        menuQuad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        menuQuad.subImage.swapchain = g.menu.handle;
        menuQuad.subImage.imageRect = {{0, 0}, {g.menu.width, g.menu.height}};
        menuQuad.pose = g.menuPose;
        menuQuad.size = {kMenuWidth, menuHeight()};
        layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&menuQuad);
      }
    }

    const PointerVisibility vis = pointerVisibility();
    bool wantBeam = g.beamReady && g.beamVisible &&
                    (vis.showRay(overlayOpen()) || (g.beamHit && vis.showMarker(overlayOpen())));
    if (wantBeam) {
      XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
      locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
      locate.displayTime = time;
      locate.space = g.localSpace;
      XrViewState viewState{XR_TYPE_VIEW_STATE};
      XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
      uint32_t viewCount = 0;
      if (XR_SUCCEEDED(xrLocateViews(g.session, &locate, &viewState, 2, &viewCount, views)) && viewCount == 2) {
        bool drawn = true;
        for (int eye = 0; eye < 2; ++eye) {
          if (!acquire(g.eyes[eye], &index)) {
            drawn = false;
            break;
          }
          drawBeamEye(views[eye], g.eyes[eye].framebuffers[index], g.eyes[eye]);
          release(g.eyes[eye]);
          beamViews[eye].pose = views[eye].pose;
          beamViews[eye].fov = views[eye].fov;
          beamViews[eye].subImage.swapchain = g.eyes[eye].handle;
          beamViews[eye].subImage.imageRect = {{0, 0}, {g.eyes[eye].width, g.eyes[eye].height}};
        }
        if (drawn) {
          beamLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
          beamLayer.space = g.localSpace;
          beamLayer.viewCount = 2;
          beamLayer.views = beamViews;
          layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&beamLayer);
        }
      }
    }
  } else if (!frameState.shouldRender) {
    ++g.notRendered;
  }

  XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
  endInfo.displayTime = time;
  endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  endInfo.layerCount = layerCount;
  endInfo.layers = layers;
  ok(xrEndFrame(g.session, &endInfo), "xrEndFrame");
  g.workSum += static_cast<double>(nowNs() - workStart) * 1e-6;
}

void xrThreadMain() {
  if (!eglMakeCurrent(gt.display, EGL_NO_SURFACE, EGL_NO_SURFACE, gt.xrContext)) {
    XLOGE("xr", "XR-Kontext nicht aktivierbar (EGL 0x%x)", eglGetError());
    s.xrFailed = true;
    return;
  }
  if (!initialize()) {
    XLOGE("xr", "OpenXR nicht verfügbar – normale Darstellung");
    s.xrFailed = true;
    eglMakeCurrent(gt.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    return;
  }
  s.xrReady = true;
  for (;;) {
    pollEvents();
    if (!g.running) {
      if (s.xrRunning.exchange(false)) s.cv.notify_all();
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }
    s.xrRunning = true;
    frame();
    updateStats();
  }
}

// ================================================================ Spielthread

bool startXr(SDL_Window* window) {
  gt.display = eglGetCurrentDisplay();
  gt.sdlContext = eglGetCurrentContext();
  if (gt.display == EGL_NO_DISPLAY || gt.sdlContext == EGL_NO_CONTEXT) {
    XLOGE("xr", "kein aktueller EGL-Kontext im Present (Renderer nicht GLES?)");
    return false;
  }
  EGLint configId = 0;
  eglQueryContext(gt.display, gt.sdlContext, EGL_CONFIG_ID, &configId);
  EGLint attribs[] = {EGL_CONFIG_ID, configId, EGL_NONE};
  EGLint numConfigs = 0;
  eglChooseConfig(gt.display, attribs, &gt.config, 1, &numConfigs);
  const char* eglExt = eglQueryString(gt.display, EGL_EXTENSIONS);
  if (!eglExt || !std::strstr(eglExt, "EGL_KHR_surfaceless_context")) {
    XLOGE("xr", "EGL_KHR_surfaceless_context fehlt – kein eigener XR-Thread möglich");
    return false;
  }
  // Gleiche GLES-Version wie der SDL-Kontext (Voraussetzung fürs Teilen). Die XR-Schicht
  // braucht GLES-3-Funktionen (Shader 300 es, glBlitFramebuffer); auf der Quest liefert auch
  // eine 2.0-Anfrage einen 3.2-Kontext – die Version steht zur Kontrolle im Log.
  EGLint clientVersion = 2;
  eglQueryContext(gt.display, gt.sdlContext, EGL_CONTEXT_CLIENT_VERSION, &clientVersion);
  const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  XLOGI("xr", "SDL-Kontext: EGL-Client %d, %s", clientVersion, glVersion ? glVersion : "?");
  const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, clientVersion, EGL_NONE};
  gt.xrContext = eglCreateContext(gt.display, gt.config, gt.sdlContext, contextAttribs);
  if (gt.xrContext == EGL_NO_CONTEXT) {
    XLOGE("xr", "geteilter XR-Kontext nicht anlegbar (EGL 0x%x)", eglGetError());
    return false;
  }

  JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
  jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
  if (!env || !activity) {
    XLOGE("xr", "JNI/Activity nicht verfügbar");
    return false;
  }
  env->GetJavaVM(&gt.vm);
  gt.activity = env->NewGlobalRef(activity);
  env->DeleteLocalRef(activity);

  // Drei geteilte Frame-Texturen in Fenstergröße, je mit FBO im SDL-Kontext.
  int w = 0, h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  s.frameW = w;
  s.frameH = h;
  {
    GlStateGuard guard(false);
    for (FrameSlot& slot : s.slots) {
      glGenTextures(1, &slot.texture);
      glBindTexture(GL_TEXTURE_2D, slot.texture);
      glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
      glGenFramebuffers(1, &slot.gameFbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, slot.gameFbo);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, slot.texture, 0);
    }
  }
  glFlush();
  XLOGI("xr", "XR-Thread startet (Spielbild %dx%d)", w, h);
  std::thread(xrThreadMain).detach();
  return true;
}

// Fertiges Spielbild (Fensterpuffer) in einen freien Platz kopieren und veröffentlichen.
void publishFrame() {
  int index = -1;
  GLsync readDone, oldWritten;
  {
    std::lock_guard<std::mutex> lock(s.m);
    for (int i = 0; i < kSlots; ++i) {
      if (i != s.latest && i != s.reading) {
        index = i;
        break;
      }
    }
    readDone = s.slots[index].readDone;
    s.slots[index].readDone = nullptr;
    oldWritten = s.slots[index].written;
    s.slots[index].written = nullptr;
  }
  if (readDone) {
    glWaitSync(readDone, 0, GL_TIMEOUT_IGNORED);
    glDeleteSync(readDone);
  }
  if (oldWritten) glDeleteSync(oldWritten);
  {
    GlStateGuard guard(false);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.slots[index].gameFbo);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0, 0, s.frameW, s.frameH, 0, 0, s.frameW, s.frameH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
  GLsync written = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();
  std::lock_guard<std::mutex> lock(s.m);
  s.slots[index].written = written;
  s.latest = index;
  ++s.published;
}

void countPresent() {
  uint64_t now = nowNs();
  std::lock_guard<std::mutex> lock(s.statsMutex);
  if (s.lastPresentNs) {
    double interval = static_cast<double>(now - s.lastPresentNs) * 1e-6;
    s.intervalSum += interval;
    s.intervalMax = std::max(s.intervalMax, interval);
    if (interval > kSlowFrameMs) {
      ++s.slowFrames;
      if (now - s.lastSlowWarnNs > 5000000000ull) {
        s.lastSlowWarnNs = now;
        XLOGW("perf", "langsamer Frame: %.0f ms zwischen zwei Spielbildern", interval);
      }
    }
  }
  s.lastPresentNs = now;
  ++s.presents;
}

}  // namespace

bool presentFrame(SDL_Renderer* renderer, SDL_Window* window) {
  if (s.xrFailed) return false;
  if (!gt.started) {
    gt.started = true;
    if (!startXr(window)) {
      s.xrFailed = true;
      return false;
    }
  }

  watchdogMainThread();
  SDL_RenderFlush(renderer);
  int ww = 0, wh = 0;
  SDL_GetWindowSize(window, &ww, &wh);
  s.windowW = ww;
  s.windowH = wh;
  s.windowId = SDL_GetWindowID(window);
  if (s.warpPending.exchange(false)) {
    SDL_WarpMouseInWindow(window, s.warpX.load(), s.warpY.load());
  }

  publishFrame();
  countPresent();

  // Takt: warten, bis die XR-Darstellung das letzte Bild übernommen hat – das Spiel rendert
  // so nicht schneller, als die Brille anzeigt (ersetzt VSync des Fensters).
  if (s.xrRunning.load()) {
    std::unique_lock<std::mutex> lock(s.m);
    s.cv.wait_for(lock, std::chrono::milliseconds(100),
                  [] { return s.consumed >= s.published || !s.xrRunning.load(); });
  } else {
    SDL_Delay(16);
  }
  return !s.xrFailed.load();
}

void onEngineLine(const char* line) {
  std::lock_guard<std::mutex> lock(s.errorMutex);
  if (s.engineError) return;
  // Lua-Fehler (stack traceback) oder Absturzmeldung: die Zeile davor ist die eigentliche Meldung.
  if (std::strstr(line, "stack traceback") || std::strstr(line, "Stratagus crashed")) {
    s.engineError = true;
    s.engineErrorText = s.lastEngineLine.empty() ? line : s.lastEngineLine;
    return;
  }
  if (*line) s.lastEngineLine = line;
}

void onEngineCursor() {
  if (!s.engineCursor.exchange(true)) XLOGI("loader", "Engine setzt Mauszeiger");
}

bool gameCursorVisible() { return s.gameCursor.load(); }

void onMovieStart() {
  s.movieRunning = true;
  uint32_t n = ++s.moviesStarted;
  XLOGI("movie", "Film %u beginnt", n);
}

void onMovieEnd() {
  s.movieRunning = false;
  XLOGI("movie", "Film endet");
}

}  // namespace xrs
