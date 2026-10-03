#!/usr/bin/env bash
# Automatische Tests der XR-Schicht (native/xr).
#
#   tests/run.sh host   # am PC (llvm-mingw, Host-SDL2 aus .build/host-stratagus, siehe docs/BUILD.md)
#   tests/run.sh quest  # auf der Quest als ARM64-Programm (NDK, Gerätebibliotheken, Gerätedaten);
#                       # startet die App NICHT, räumt /data/local/tmp danach auf
#
# test_geometry, test_input und test_game sind abhängigkeitsfrei; test_ui braucht SDL2/SDL2_image/FreeType
# und die Spieldaten (data.Wargus) für Menügrafiken und -schrift; am PC zusätzlich mit dem War1gus-Datensatz
# (original-data/war1gus-quest, falls vorhanden; PNGs unter png/war1gus). PNGs der Anzeigen landen in
# .build/test-out/<ziel>/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${1:-host}"
X="$ROOT/native/xr"
T="$ROOT/tests"
OUT="$ROOT/.build/test-out/$TARGET"
rm -rf "$OUT" && mkdir -p "$OUT/png"
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }
UI_SRC=("$T/test_ui.cpp" "$X/xr_canvas.cpp" "$X/xr_theme.cpp" "$X/xr_menu.cpp" "$X/xr_keyboard.cpp"
	"$X/xr_loader.cpp" "$X/xr_settings.cpp")
OPENXR_INC="$ROOT/.build/quest/prefix/openxr/include"
[[ -f "$OPENXR_INC/openxr/openxr.h" ]] || { echo "OpenXR-Header fehlen – zuerst scripts/build-native.sh" >&2; exit 1; }
FAILED=0

case "$TARGET" in
host)
	MINGW="$(ls -d "$ROOT"/.build/tools/llvm-mingw-*/bin | tail -1)"
	export PATH="$MINGW:$PATH"
	H="$ROOT/.build/host-stratagus"
	FLAGS=(-std=c++17 -O1 -Wall -Wextra -Werror -Wno-missing-field-initializers -I"$X" -I"$OPENXR_INC")
	# Beide Apps: Label (strings.xml je Flavor) = App-ID (build.gradle) = Profil (xr_game.h)
	for flavor in stratagus war1gus; do
		label="$(sed -n 's/.*name="app_name">\([^<]*\)<.*/\1/p' "$ROOT/android/src/$flavor/res/values/strings.xml")"
		if grep -A3 "^        $flavor {" "$ROOT/android/build.gradle" | grep -q "applicationId '$label'" &&
			grep -q "\"$label\"" "$X/xr_game.h"; then
			echo "Flavor $flavor: Label = App-ID = Profil ($label)"
		else
			echo "FEHLER: Flavor $flavor: Label '$label' passt nicht zu App-ID/Profil"; FAILED=1
		fi
	done
	for t in geometry input game; do
		clang++ "${FLAGS[@]}" -static -o "$OUT/test_$t.exe" "$T/test_$t.cpp"
		"$OUT/test_$t.exe" || FAILED=1
	done
	[[ -f "$ROOT/.build/host-freetype/libfreetype.a" ]] || bash "$ROOT/tools/menu-preview/build.sh" >/dev/null
	clang++ "${FLAGS[@]}" -Wno-error -I"$ROOT/tools/menu-preview/shim" \
		-I"$H/SDL2/src/SDL2-build/include" -I"$H/SDL2/src/SDL2-build/include/SDL2" \
		-I"$ROOT/upstream/stratagus/third-party/SDL/include" -I"$ROOT/upstream/stratagus/third-party/SDL_image" \
		-I"$ROOT/external/freetype/include" -o "$OUT/test_ui.exe" "${UI_SRC[@]}" \
		"$H/SDL2_image/src/SDL2_image-build/libSDL2_image.a" "$H/png/src/png-build/libpng16.a" \
		"$H/zlib/src/zlib-build/libzlibstatic.a" "$H/jpeg/src/jpeg-build/libjpeg.a" \
		"$H/SDL2/src/SDL2-build/libSDL2.a" "$ROOT/.build/host-freetype/libfreetype.a" \
		-static -lsetupapi -lwinmm -limm32 -lversion -lole32 -loleaut32 -lgdi32 -luser32 -lcfgmgr32 -luuid \
		-ladvapi32 -lshell32
	"$OUT/test_ui.exe" "$(native "$ROOT/original-data/wargus-quest")" "$(native "$OUT")" "$(native "$OUT/png")" \
		|| FAILED=1
	if [[ -f "$ROOT/original-data/war1gus-quest/extracted" ]]; then
		mkdir -p "$OUT/png/war1gus" "$OUT/war1gus"
		XRS_GAME=war1gus "$OUT/test_ui.exe" "$(native "$ROOT/original-data/war1gus-quest")" "$(native "$OUT/war1gus")" \
			"$(native "$OUT/png/war1gus")" || FAILED=1
	else
		echo "Hinweis: original-data/war1gus-quest fehlt – test_ui für War1gus übersprungen (scripts/prepare-data-war1.sh)"
	fi
	;;
quest)
	SDK="${ANDROID_HOME:-$ROOT/../_tools/android-sdk}"
	NDK="${ANDROID_NDK:-$SDK/ndk/30.0.16248370}"
	CXX="$(ls "$NDK"/toolchains/llvm/prebuilt/*/bin/clang++* | head -1)"
	ADB="${ADB:-$SDK/platform-tools/adb.exe}"
	[[ -x "$ADB" ]] || ADB="$SDK/platform-tools/adb"
	P="$ROOT/.build/quest/prefix"
	FLAGS=(--target=aarch64-linux-android29 -std=c++17 -O1 -Wall -Wextra -Werror -Wno-missing-field-initializers
		-fPIE -pie -static-libstdc++ -I"$X" -I"$OPENXR_INC")
	for t in geometry input game; do
		"$CXX" "${FLAGS[@]}" -o "$OUT/test_$t" "$T/test_$t.cpp"
	done
	"$CXX" "${FLAGS[@]}" -I"$P/include/SDL2" -I"$P/include/freetype2" -o "$OUT/test_ui" "${UI_SRC[@]}" \
		"$P/lib/libSDL2_image.a" "$P/lib/libfreetype.a" -L"$P/lib" -lSDL2 -llog -landroid
	# Bildfilter auf der GPU (EGL-Pbuffer) und Filmton-Fix (music_hook/tracked_rwops) gegen echte Filme
	"$CXX" "${FLAGS[@]}" -o "$OUT/test_filter_gpu" "$T/test_filter_gpu.cpp" "$X/xr_gl.cpp" -lEGL -lGLESv3
	"$CXX" "${FLAGS[@]}" -I"$P/include/SDL2" -I"$ROOT/native/src" -I"$ROOT/external/SDL_mixer/external/vorbis/include" \
		-I"$ROOT/external/SDL_mixer/external/ogg/include" -I"$ROOT/.build/quest/SDL_mixer/external/ogg/include" \
		-o "$OUT/test_music_hook" "$T/test_music_hook.cpp" "$P/lib/libvorbisfile.a" "$P/lib/libvorbis.a" \
		"$P/lib/libogg.a" -L"$P/lib" -lSDL2 -lm
	DEV=/data/local/tmp/xr-stratagus-tests
	DATA=/sdcard/Android/data/xr.wargus2/files/data.Wargus
	export MSYS_NO_PATHCONV=1
	"$ADB" shell "rm -rf $DEV && mkdir -p $DEV/png $DEV/work"
	for f in test_geometry test_input test_game test_ui test_filter_gpu test_music_hook; do "$ADB" push "$(native "$OUT/$f")" "$DEV/" >/dev/null; done
	"$ADB" push "$(native "$P/lib/libSDL2.so")" "$DEV/" >/dev/null
	"$ADB" push "$(native "$(dirname "$CXX")/../sysroot/usr/lib/aarch64-linux-android/libc++_shared.so")" "$DEV/" \
		>/dev/null
	"$ADB" shell "cd $DEV && chmod 755 test_* && ./test_geometry && ./test_input && ./test_game && \
		LD_LIBRARY_PATH=$DEV ./test_ui $DATA $DEV/work $DEV/png && ./test_filter_gpu $DEV/png && \
		LD_LIBRARY_PATH=$DEV ./test_music_hook $DATA/videos/logo.ogv $DATA/videos/gameintro.ogv" || FAILED=1
	"$ADB" pull "$DEV/png/." "$(native "$OUT/png")" >/dev/null || true
	"$ADB" shell "rm -rf $DEV"
	;;
*)
	sed -n '2,10p' "$0"; exit 2
	;;
esac

echo "PNG-Ausgaben: $OUT/png"
if [[ $FAILED -ne 0 ]]; then echo "TESTS FEHLGESCHLAGEN ($TARGET)"; exit 1; fi
echo "Alle Tests bestanden ($TARGET)"
