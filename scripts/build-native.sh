#!/usr/bin/env bash
# Baut die nativen Bibliotheken für die Quest (arm64-v8a) und legt sie nach
# android/src/main/jniLibs/arm64-v8a.
#
#   Stufe 0: tolua++ für den Host (erzeugt C++-Bindings aus src/tolua/*.pkg)
#   Stufe 1: SDL2 (shared), SDL2_image, SDL2_mixer (static) → .build/quest/prefix
#   Stufe 2: Stratagus + vendored Lua/zlib/png/bzip2/ogg/vorbis/theora → libmain.so
#
# Umgebung: ANDROID_HOME (sonst das gemeinsame SDK ../_tools/android-sdk neben dem Projekt),
# optional ANDROID_NDK, XR_CMAKE_VERSION (Standard 3.31.6), HOST_CC/HOST_CXX.
# Windows: Host-Compiler = llvm-mingw unter .build/tools (siehe docs/BUILD.md).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/.build/quest"
JOBS="${JOBS:-8}"

to_native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }
to_unix()   { if command -v cygpath >/dev/null 2>&1; then cygpath -u "$1"; else echo "$1"; fi; }

# Nicht %LOCALAPPDATA%: Claude Desktop (MSIX) leitet das in eine Sandbox um.
if [[ -z "${ANDROID_HOME:-}" && -d "$ROOT/../_tools/android-sdk" ]]; then
	ANDROID_HOME="$(cd "$ROOT/../_tools/android-sdk" && pwd)"
fi
ANDROID_HOME="$(to_unix "${ANDROID_HOME:?ANDROID_HOME setzen}")"
export ANDROID_NDK="${ANDROID_NDK:-$ANDROID_HOME/ndk/30.0.16248370}"
ANDROID_NDK="$(to_native "$(to_unix "$ANDROID_NDK")")"
CMAKE_BIN="$ANDROID_HOME/cmake/${XR_CMAKE_VERSION:-3.31.6}/bin"
[[ -x "$CMAKE_BIN/cmake" || -x "$CMAKE_BIN/cmake.exe" ]] || {
	echo "CMake ${XR_CMAKE_VERSION:-3.31.6} fehlt im SDK (android sdk install cmake/3.31.6)" >&2; exit 1; }
export PATH="$CMAKE_BIN:$PATH"

TOOLCHAIN="$(to_native "$ROOT/cmake/quest-arm64.toolchain.cmake")"
PREFIX="$BUILD/prefix"
PREFIX_N="$(to_native "$PREFIX")"
JNILIBS="$ROOT/android/src/main/jniLibs/arm64-v8a"

# ---- Stufe 0: Host-tolua++ ----
HOST_BUILD="$ROOT/.build/host-lua"
TOLUAPP="$HOST_BUILD/toluapp"
[[ -f "$TOLUAPP.exe" ]] && TOLUAPP="$TOLUAPP.exe"
if [[ ! -f "$TOLUAPP" ]]; then
	MINGW="$(ls -d "$ROOT"/.build/tools/llvm-mingw-*/bin 2>/dev/null | tail -1 || true)"
	[[ -n "$MINGW" ]] && export PATH="$MINGW:$PATH"
	HOST_CC="${HOST_CC:-clang}"; HOST_CXX="${HOST_CXX:-clang++}"
	cmake -S "$ROOT/upstream/stratagus/third-party/lua-5.1.5" -B "$HOST_BUILD" -G Ninja \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$HOST_CC" -DCMAKE_CXX_COMPILER="$HOST_CXX"
	# Upstream kopiert lua.h erst nach dem Lua-Link; tolua++ zuerst bauen scheitert parallel
	cmake --build "$HOST_BUILD" --target lua51_static
	cmake --build "$HOST_BUILD" --target toluapp
	TOLUAPP="$HOST_BUILD/toluapp"; [[ -f "$TOLUAPP.exe" ]] && TOLUAPP="$TOLUAPP.exe"
fi

android_cmake() { # <quelle> <build> [args…]
	local src="$1" bld="$2"; shift 2
	cmake -S "$src" -B "$bld" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
		-DCMAKE_INSTALL_PREFIX="$PREFIX_N" -DCMAKE_PREFIX_PATH="$PREFIX_N" \
		-DCMAKE_FIND_ROOT_PATH="$PREFIX_N" "$@"
}

# ---- Stufe 1: SDL2-Familie ----
if [[ ! -f "$PREFIX/lib/libSDL2.so" ]]; then
	android_cmake "$ROOT/external/SDL" "$BUILD/SDL" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF
	cmake --build "$BUILD/SDL" -j"$JOBS" && cmake --install "$BUILD/SDL"
fi
if [[ ! -f "$PREFIX/lib/libSDL2_image.a" ]]; then
	android_cmake "$ROOT/external/SDL_image" "$BUILD/SDL_image" -DBUILD_SHARED_LIBS=OFF \
		-DSDL2_DIR="$PREFIX_N/lib/cmake/SDL2" -DSDL2IMAGE_VENDORED=OFF -DSDL2IMAGE_SAMPLES=OFF \
		-DSDL2IMAGE_BACKEND_STB=ON -DSDL2IMAGE_AVIF=OFF -DSDL2IMAGE_JXL=OFF -DSDL2IMAGE_TIF=OFF \
		-DSDL2IMAGE_WEBP=OFF -DSDL2IMAGE_DEPS_SHARED=OFF
	cmake --build "$BUILD/SDL_image" -j"$JOBS" && cmake --install "$BUILD/SDL_image"
fi
if [[ ! -f "$PREFIX/lib/libSDL2_mixer.a" || ! -f "$PREFIX/lib/libvorbisfile.a" ]]; then
	# WAV, OGG über libvorbisfile (stb_vorbis liest keine Ogg-Dateien mit Theora-Spur → Videos
	# ohne Ton), MIDI über eingebautes TiMidity. Vendored sind nur ogg/vorbis, alles andere aus.
	git -C "$ROOT/external/SDL_mixer" submodule update --init external/ogg external/vorbis
	rm -rf "$BUILD/SDL_mixer"
	android_cmake "$ROOT/external/SDL_mixer" "$BUILD/SDL_mixer" -DBUILD_SHARED_LIBS=OFF \
		-DSDL2_DIR="$PREFIX_N/lib/cmake/SDL2" -DSDL2MIXER_VENDORED=ON -DSDL2MIXER_SAMPLES=OFF \
		-DSDL2MIXER_DEPS_SHARED=OFF -DSDL2MIXER_FLAC=OFF -DSDL2MIXER_MOD=OFF -DSDL2MIXER_MP3=OFF \
		-DSDL2MIXER_OPUS=OFF -DSDL2MIXER_WAVPACK=OFF -DSDL2MIXER_GME=OFF \
		-DSDL2MIXER_MIDI_FLUIDSYNTH=OFF -DSDL2MIXER_MIDI_TIMIDITY=ON \
		-DSDL2MIXER_VORBIS=VORBISFILE
	cmake --build "$BUILD/SDL_mixer" -j"$JOBS" && cmake --install "$BUILD/SDL_mixer"
fi

if [[ ! -f "$PREFIX/lib/libfreetype.a" ]]; then
	# Systemschrift der Quest für VR-Menü/Ladeanzeige (Ersatz, falls Spielgrafiken fehlen)
	android_cmake "$ROOT/external/freetype" "$BUILD/freetype" -DBUILD_SHARED_LIBS=OFF \
		-DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON \
		-DFT_DISABLE_BROTLI=ON
	cmake --build "$BUILD/freetype" -j"$JOBS" && cmake --install "$BUILD/freetype"
fi

# OpenXR-Loader: offizielles Khronos-AAR (dieselbe Version wie in android/build.gradle, das die
# .so und die Manifest-Einträge in die APK bringt). Hier nur Header + .so zum Linken.
OPENXR_VERSION=1.1.58
OPENXR_SHA256=7aca6ea1c717d55735e62e93bea6f1339e57b785f3c8f1ad11e84a4210526aa4
OPENXR_DIR="$PREFIX/openxr"
if [[ ! -f "$OPENXR_DIR/lib/libopenxr_loader.so" ]]; then
	AAR="$ROOT/.build/downloads/openxr_loader_for_android-$OPENXR_VERSION.aar"
	mkdir -p "$(dirname "$AAR")"
	[[ -f "$AAR" ]] || curl -sSLf --retry 3 -o "$AAR" \
		"https://repo1.maven.org/maven2/org/khronos/openxr/openxr_loader_for_android/$OPENXR_VERSION/openxr_loader_for_android-$OPENXR_VERSION.aar"
	echo "$OPENXR_SHA256  $AAR" | sha256sum -c --quiet -
	TMP="$BUILD/openxr-aar"
	rm -rf "$TMP" && mkdir -p "$TMP"
	unzip -q -o "$AAR" -d "$TMP"
	mkdir -p "$OPENXR_DIR/include" "$OPENXR_DIR/lib"
	cp -r "$TMP/prefab/modules/headers/include/openxr" "$OPENXR_DIR/include/"
	cp "$TMP/jni/arm64-v8a/libopenxr_loader.so" "$OPENXR_DIR/lib/"
	rm -rf "$TMP"
fi

# ---- Stufe 2: Stratagus ----
android_cmake "$ROOT/native" "$BUILD/stratagus" -DXR_DEPS_PREFIX="$PREFIX_N" \
	-DSTRATAGUS_HOST_TOLUAPP="$(to_native "$TOLUAPP")"
# Die ExternalProjects melden ihre .a-Dateien nicht als Byproducts, Ninja kennt für libmain.so
# also keine Regel dafür: erst stratagus_lib (zieht die Externals per add_dependencies), dann main.
# Das Upstream-Lua-Unterprojekt hat dieselbe Header-Race wie oben: bei Fehlschlag einmal wiederholen.
cmake --build "$BUILD/stratagus" --target stratagus_lib -j"$JOBS" \
	|| cmake --build "$BUILD/stratagus" --target stratagus_lib -j"$JOBS"
cmake --build "$BUILD/stratagus" --target main -j"$JOBS"

mkdir -p "$JNILIBS"
cp "$BUILD/stratagus/libmain.so" "$PREFIX/lib/libSDL2.so" "$JNILIBS/"
# libopenxr_loader.so kommt über die Gradle-Abhängigkeit (AAR) in die APK, nicht von hier
cp "$(to_unix "$ANDROID_NDK")/toolchains/llvm/prebuilt/"*/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so "$JNILIBS/"
echo "Fertig: $(ls "$JNILIBS")"

# Symbol-Kollision prüfen: libmain.so darf keine Daten (B/D) unter Namen von GL-/EGL-/OpenXR-
# Funktionen enthalten – sonst ruft die XR-Schicht statt der Funktion eine Variable auf (so
# geschehen mit Stratagus' shaders.cpp, siehe native/CMakeLists.txt).
NM="$(ls "$(to_unix "$ANDROID_NDK")"/toolchains/llvm/prebuilt/*/bin/llvm-nm* | head -1)"
SHADOW="$("$NM" "$JNILIBS/libmain.so" | grep -E ' [BbDd] (gl|egl|xr)[A-Z]' || true)"
if [[ -n "$SHADOW" ]]; then
	echo "FEHLER: libmain.so überdeckt Grafik-/XR-Funktionen mit Variablen:" >&2
	echo "$SHADOW" >&2
	exit 1
fi
echo "Symbolprüfung: keine überdeckten GL/EGL/OpenXR-Funktionen"
