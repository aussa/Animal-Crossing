# =============================================================================
# Animal Crossing PC Port - Dependencies Configuration
# =============================================================================

# =============================================================================
# SDL3 (input, audio, window, events)
# =============================================================================
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
    if(APPLE)
        set(SDL3_DIR "/opt/homebrew/lib/cmake/SDL3" CACHE PATH "SDL3 cmake dir")
    endif()
    find_package(SDL3 CONFIG QUIET)
endif()
if(NOT SDL3_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL3 QUIET sdl3)
    endif()
endif()
if(NOT SDL3_FOUND)
    message(FATAL_ERROR
        "SDL3 not found!\n"
        "  macOS:         brew install sdl3\n"
        "  Linux:         install libsdl3-dev from your distro\n"
        "  Windows/MSYS2: pacman -S mingw-w64-sdl3\n"
        "  Or set -DSDL3_DIR=<path/to/SDL3/cmake>")
endif()
message(STATUS "SDL3 found")

# =============================================================================
# GLAD (OpenGL loader)
# =============================================================================
add_library(glad STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/gl.c
)
target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)
