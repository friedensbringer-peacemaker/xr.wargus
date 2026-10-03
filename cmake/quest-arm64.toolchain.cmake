# Toolchain-Wrapper für die Quest (arm64-v8a).
#
# Stratagus baut seine mitgelieferten Abhängigkeiten (third-party/) als ExternalProject und
# reicht dabei nur CMAKE_TOOLCHAIN_FILE weiter, nicht ANDROID_ABI/ANDROID_PLATFORM. Ohne
# diesen Wrapper fallen die Unterprojekte auf die NDK-Standards (armv7, API 21) zurück.

set(ANDROID_ABI arm64-v8a CACHE STRING "")
set(ANDROID_PLATFORM android-29 CACHE STRING "")
set(ANDROID_STL c++_shared CACHE STRING "")
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "")
endif()

if(NOT DEFINED ANDROID_NDK)
	if(DEFINED ENV{ANDROID_NDK})
		set(ANDROID_NDK "$ENV{ANDROID_NDK}")
	elseif(DEFINED ENV{ANDROID_NDK_ROOT})
		set(ANDROID_NDK "$ENV{ANDROID_NDK_ROOT}")
	else()
		message(FATAL_ERROR "ANDROID_NDK (Umgebungsvariable) muss auf NDK 30 zeigen")
	endif()
endif()
file(TO_CMAKE_PATH "${ANDROID_NDK}" ANDROID_NDK)

# Der (Standard-)Legacy-Pfad des NDK-Toolchain-Files setzt CMAKE_<LANG>_FLAGS als normale
# Variablen; danach angehängte Flags landen deshalb in jedem (Unter-)Projekt.
include("${ANDROID_NDK}/build/cmake/android.toolchain.cmake")

# NDK r30 linkt mit --no-undefined-version; die zlib-Versionsskripte der Abhängigkeiten
# nennen Symbole, die es in dieser Konfiguration nicht gibt.
string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--undefined-version")
# Lua 5.1 legt String-Inhalte direkt hinter dem TString-Kopf ab (svalue = ts+1). Bionic-FORTIFY
# hält strchr() darauf für einen Überlauf und bricht ab (lgc.c traversetable). Nur für das
# vendored-Lua-Unterprojekt abschalten.
if(CMAKE_SOURCE_DIR MATCHES "lua-5\\.1\\.5/?$")
	string(APPEND CMAKE_C_FLAGS " -U_FORTIFY_SOURCE")
	string(APPEND CMAKE_CXX_FLAGS " -U_FORTIFY_SOURCE")
endif()
# 16-KB-Seitenausrichtung (Android 15+/Quest)
string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,-z,max-page-size=16384")
