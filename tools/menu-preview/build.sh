#!/usr/bin/env bash
# Baut und startet die Desktop-Vorschau der VR-Anzeigen (Windows, llvm-mingw).
# Voraussetzung: Host-Stratagus-Build (.build/host-stratagus, siehe docs/BUILD.md „Desktop-Gegenprobe“)
# für SDL2/SDL2_image/libpng/zlib und Host-FreeType (.build/host-freetype).
#   tools/menu-preview/build.sh [data.Wargus] [Ausgabeordner]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DATA="${1:-$ROOT/original-data/wargus-quest}"
OUT="${2:-$ROOT/.build/menu-preview}"
MINGW="$(ls -d "$ROOT"/.build/tools/llvm-mingw-*/bin | tail -1)"
export PATH="$MINGW:$PATH"
H="$ROOT/.build/host-stratagus"
X="$ROOT/native/xr"
mkdir -p "$OUT"
if [[ ! -f "$ROOT/.build/host-freetype/libfreetype.a" ]]; then
	cmake -S "$ROOT/external/freetype" -B "$ROOT/.build/host-freetype" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER=clang -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON \
		-DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
	cmake --build "$ROOT/.build/host-freetype"
fi
clang++ -std=c++17 -O1 -Wall -Wextra -o "$OUT/menu-preview.exe" \
	-I"$ROOT/tools/menu-preview/shim" -I"$X" \
	-I"$H/SDL2/src/SDL2-build/include" -I"$H/SDL2/src/SDL2-build/include/SDL2" \
	-I"$ROOT/upstream/stratagus/third-party/SDL/include" -I"$ROOT/upstream/stratagus/third-party/SDL_image" \
	-I"$ROOT/external/freetype/include" \
	"$ROOT/tools/menu-preview/main.cpp" "$X/xr_canvas.cpp" "$X/xr_theme.cpp" "$X/xr_menu.cpp" \
	"$X/xr_keyboard.cpp" "$X/xr_loader.cpp" "$X/xr_settings.cpp" \
	"$H/SDL2_image/src/SDL2_image-build/libSDL2_image.a" "$H/png/src/png-build/libpng16.a" \
	"$H/zlib/src/zlib-build/libzlibstatic.a" "$H/jpeg/src/jpeg-build/libjpeg.a" \
	"$H/SDL2/src/SDL2-build/libSDL2.a" "$ROOT/.build/host-freetype/libfreetype.a" \
	-static -lsetupapi -lwinmm -limm32 -lversion -lole32 -loleaut32 -lgdi32 -luser32 -lcfgmgr32 -luuid \
	-ladvapi32 -lshell32
"$OUT/menu-preview.exe" "$(cygpath -m "$DATA")" "$(cygpath -m "$OUT")" "C:/Windows/Fonts/arial.ttf"
