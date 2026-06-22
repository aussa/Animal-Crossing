# =============================================================================
# Animal Crossing PC Port - Compiler Definitions and Warnings
# =============================================================================

add_compile_definitions(
    TARGET_PC
    VERSION=${AC_VERSION}
    F3DEX_GBI_2
    NDEBUG
    BUGFIXES       # Fix original game bugs (buffer overflows, etc.) that crash on modern platforms
    _LANGUAGE_C    # Required for N64 GBI/ultratypes headers
    PC_ENHANCEMENTS   # Optional visual improvements (MSAA, etc). Remove to get pure PC port.
    KEYBOARD_TYPING   # Physical keyboard typing in text editor. Remove for platforms without keyboards.
    MOUSE_INPUT       # Mouse input for menu and misc interfaces. Remove for platforms without mouse.
    SDL_ENABLE_OLD_NAMES
)

if(AC_USE_RAINFALL)
    add_compile_definitions(AC_USE_RAINFALL HAS_SDL3=1 HAS_OPENGL=1)
    if(AC_RAINFALL_BISECT_NO_FLUSH)
        add_compile_definitions(AC_RAINFALL_BISECT_NO_FLUSH=1)
    endif()
    if(AC_RAINFALL_BISECT_NO_TEXTURE_BIND)
        add_compile_definitions(AC_RAINFALL_BISECT_NO_TEXTURE_BIND=1)
    endif()
    if(AC_RAINFALL_BISECT_LEGACY_GXWGFIFO)
        add_compile_definitions(AC_RAINFALL_BISECT_LEGACY_GXWGFIFO=1)
    endif()
endif()

if(APPLE)
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif()

# Common compiler flags (applied to all sources)
if(MSVC)
    add_compile_options(/W0)
else()
    add_compile_options(
        -fno-strict-aliasing
        -fwrapv
    )
endif()

# =============================================================================
# Decompiled Code Compiler Warnings / Settings
# =============================================================================
# Suppress warnings (fires heavily on decompiled code)
# Clang treats some decomp patterns as errors even with -w:
# -Wno-return-type: missing return values in non-void functions
# -Wno-initializer-overrides: GBI macros use designated initializers with overlapping fields
set(DECOMP_WARN_FLAGS "-w;-fpermissive;-Wno-return-type;-Wno-error=return-type")

if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT MSVC)
    list(APPEND DECOMP_WARN_FLAGS "-Wno-pointer-to-int-cast" "-Wno-int-to-pointer-cast")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-Wno-register>)
    list(APPEND DECOMP_WARN_FLAGS "-Wno-c99-designator" "-Wno-initializer-overrides" "-Wno-register")
endif()

# Applied in game.cmake after sources are collected

# =============================================================================
# Rename main() entry points
# =============================================================================
# Rename game's main() to ac_entry() to avoid conflict with PC main() and game_main(GAME*)
set_source_files_properties(
    "${DECOMP_ROOT}/src/main.c"
    PROPERTIES COMPILE_DEFINITIONS "main=ac_entry"
)

# Rename boot.c's main() to avoid conflict with PC main()
set_source_files_properties(
    "${DECOMP_ROOT}/src/static/boot.c"
    PROPERTIES COMPILE_DEFINITIONS "main=boot_main"
)
