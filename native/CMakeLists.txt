# Stufe 2 des Quest-Builds: Stratagus als libmain.so für SDLActivity.
#
# Erwartet die in Stufe 1 (scripts/build-native.sh) installierten SDL2-Bibliotheken unter
# XR_DEPS_PREFIX. Lua, zlib, libpng, bzip2, Ogg/Vorbis und Theora kommen aus Stratagus'
# third-party (BUILD_VENDORED_*), SDL2 bewusst nicht: dessen 2.0.22 kompiliert nicht mehr
# gegen aktuelle NDKs (ALooper_pollAll entfernt).

cmake_minimum_required(VERSION 3.25)
project(xr_stratagus C CXX)

if(NOT ANDROID)
	message(FATAL_ERROR "native/ ist nur für den Android-Build gedacht")
endif()
if(NOT XR_DEPS_PREFIX)
	message(FATAL_ERROR "XR_DEPS_PREFIX fehlt (Installationspfad aus Stufe 1)")
endif()

set(XR_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

# Vorbelegung der Stratagus-Find-Module mit den Stufe-1-Artefakten
set(SDL2_INCLUDE_DIR "${XR_DEPS_PREFIX}/include/SDL2" CACHE PATH "")
set(SDL2_LIBRARY_TEMP "${XR_DEPS_PREFIX}/lib/libSDL2.so" CACHE FILEPATH "")
set(SDL2main_LIBRARY "" CACHE FILEPATH "")
set(SDL2_MIXER_INCLUDE_DIR "${XR_DEPS_PREFIX}/include/SDL2" CACHE PATH "")
# SDL_mixer ist statisch mit vendored libvorbisfile gebaut; dessen Archive gehören in die Linkliste
set(SDL2_MIXER_LIBRARY "${XR_DEPS_PREFIX}/lib/libSDL2_mixer.a;${XR_DEPS_PREFIX}/lib/libvorbisfile.a;${XR_DEPS_PREFIX}/lib/libvorbis.a;${XR_DEPS_PREFIX}/lib/libogg.a" CACHE STRING "" FORCE)
set(SDL2_IMAGE_INCLUDE_DIR "${XR_DEPS_PREFIX}/include/SDL2" CACHE PATH "")
set(SDL2_IMAGE_LIBRARY "${XR_DEPS_PREFIX}/lib/libSDL2_image.a" CACHE FILEPATH "")

set(BUILD_VENDORED_LUA ON CACHE BOOL "" FORCE)
set(BUILD_VENDORED_SDL OFF CACHE BOOL "" FORCE)
set(BUILD_VENDORED_MEDIA_LIBS ON CACHE BOOL "" FORCE)
set(WITH_MNG OFF CACHE BOOL "" FORCE)
set(WITH_OPENMP OFF CACHE BOOL "" FORCE)
set(WITH_STACKTRACE OFF CACHE BOOL "" FORCE)

add_subdirectory("${XR_ROOT}/upstream/stratagus" stratagus EXCLUDE_FROM_ALL)

# src/video/shaders.cpp definiert für die Desktop-OpenGL-Shader globale Funktionszeiger mit den
# Namen echter GL-Funktionen (glUseProgram, glCreateShader, …). In libmain.so überdecken diese
# Variablen die Funktionen aus libGLESv3: jeder Aufruf der XR-Schicht sprang in .bss (SIGSEGV
# beim ersten Frame, 0.2.0/0.2.1). Nur dort umbenennen; auf GLES werden die Shader nicht benutzt.
set(STRATAGUS_GL_SHADOWED glCreateShader glShaderSource glCompileShader glGetShaderiv glGetShaderInfoLog
	glDeleteShader glAttachShader glCreateProgram glLinkProgram glValidateProgram glGetProgramiv
	glGetProgramInfoLog glUseProgram glGetUniformLocation glUniform1i glUniform1f glUniform2f
	glUniformMatrix4fv glGetAttribLocation glVertexAttrib4f)
set(STRATAGUS_GL_RENAMES)
foreach(fn IN LISTS STRATAGUS_GL_SHADOWED)
	list(APPEND STRATAGUS_GL_RENAMES "${fn}=stratagus_${fn}")
endforeach()
set_source_files_properties("${XR_ROOT}/upstream/stratagus/src/video/shaders.cpp"
	DIRECTORY "${XR_ROOT}/upstream/stratagus"
	PROPERTIES COMPILE_DEFINITIONS "${STRATAGUS_GL_RENAMES}")

# SDLActivity lädt libmain.so und ruft SDL_main (SDL.h benennt main um). Eigener Einstieg statt
# upstream main.cpp: Datenpfade, logcat-Umleitung.
# XR-Schicht (xr/, übernommen aus XR-CorsixTH): OpenXR-Thread, Spielfenster als Quad, Strahl,
# VR-Menü im Wargus-Stil, VR-Tastatur, Ladeanzeige, Passthrough.
set(XR_SOURCES
	xr/xr_app.cpp xr/xr_present_hook.cpp xr/xr_log.cpp xr/xr_settings.cpp xr/xr_gl.cpp xr/xr_canvas.cpp
	xr/xr_theme.cpp xr/xr_loader.cpp xr/xr_keyboard.cpp xr/xr_menu.cpp xr/xr_watchdog.cpp)
add_library(main SHARED src/android_main.cpp src/cursor_hook.cpp src/music_hook.cpp ${XR_SOURCES})
# Eigener Code warnungsfrei; {XR_TYPE_X}-Idiom der OpenXR-Structs ausgenommen.
target_compile_options(main PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
# include_directories()/add_definitions() im Upstream gelten nur für dessen Verzeichnis
get_directory_property(STRATAGUS_DEFINITIONS DIRECTORY "${XR_ROOT}/upstream/stratagus" COMPILE_DEFINITIONS)
target_compile_definitions(main PRIVATE ${STRATAGUS_DEFINITIONS})
target_include_directories(main PRIVATE "${XR_ROOT}/upstream/stratagus/src/include" "${SDL2_INCLUDE_DIR}")
target_include_directories(main SYSTEM PRIVATE "${XR_DEPS_PREFIX}/openxr/include" "${XR_DEPS_PREFIX}/include/freetype2")

add_library(openxr_loader SHARED IMPORTED)
set_target_properties(openxr_loader PROPERTIES IMPORTED_LOCATION "${XR_DEPS_PREFIX}/openxr/lib/libopenxr_loader.so")

target_link_libraries(main PRIVATE stratagus_lib "${XR_DEPS_PREFIX}/lib/libfreetype.a" openxr_loader
	android log EGL GLESv3 OpenSLES)
target_link_options(main PRIVATE "-Wl,--no-undefined"
	# Engine-Aufrufe auf die XR-Schicht umleiten (keine Engine-Änderung nötig), xr_present_hook.cpp
	"-Wl,--wrap=SDL_RenderPresent" "-Wl,--wrap=SDL_SetCursor"
	"-Wl,--wrap=SDL_CreateTexture" "-Wl,--wrap=SDL_DestroyTexture"
	# Spiel-Mauszeiger ausblendbar (src/cursor_hook.cpp): DrawCursor()/HideCursor() aus Stratagus
	"-Wl,--wrap=_Z10DrawCursorv" "-Wl,--wrap=_Z10HideCursorv"
	# Filmton als Datenstrom öffnen (src/music_hook.cpp): vorbisfile hing sonst in der Rückwärtssuche
	"-Wl,--wrap=Mix_LoadMUS" "-Wl,--wrap=Mix_LoadMUS_RW")
