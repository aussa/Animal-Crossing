# =============================================================================
# Animal Crossing PC Port - CMake Build Configuration
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(ac_pc C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Options
option(PC_CONSOLE "Show console window (disables -mwindows, useful for debugging)" OFF)

# Game version: 0=USA (GAFE01_00), 1=AUS (GAFU01_00), 2=JPN (GAFJ01_00)
set(AC_VERSION 0 CACHE STRING "Animal Crossing version (0=USA, 1=AUS, 2=JPN)")
set_property(CACHE AC_VERSION PROPERTY STRINGS 0 1 2)

# Set mixed linkage permissions
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Decomp project root directory
set(DECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# =============================================================================
# Subsystem Modules (More closely matching Courage-Reborn)
# =============================================================================
include(cmake/dependencies.cmake)
include(cmake/compile_defs.cmake)
include(cmake/dolphin.cmake)
include(cmake/game.cmake)
include(cmake/fixnes.cmake)

# =============================================================================
# Windows resource file
# =============================================================================
if(WIN32)
    set(RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc")
endif()

# =============================================================================
# Build Executable Target
# =============================================================================
add_executable(ac_pc
    ${PC_SOURCES}
    ${FIXNES_SOURCES}
    ${GAME_C_SOURCES}
    ${GAME_CXX_SOURCES}
    ${RC_FILE}
)

target_include_directories(ac_pc PRIVATE
    ${DECOMP_ROOT}/include
    ${DECOMP_ROOT}/src
    ${DECOMP_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/include/render
    ${CMAKE_CURRENT_SOURCE_DIR}/include/os
    ${CMAKE_CURRENT_SOURCE_DIR}/include/input
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utility
    ${CMAKE_CURRENT_SOURCE_DIR}/include/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/include/menus
    ${CMAKE_CURRENT_SOURCE_DIR}/external/bc7decomp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/fixnes
    ${SDL2_INCLUDE_DIRS}
)

# Link SDL2 without SDL2main (we define SDL_MAIN_HANDLED)
if(TARGET SDL2::SDL2)
    target_link_libraries(ac_pc glad SDL2::SDL2)
else()
    list(FILTER SDL2_LIBRARIES EXCLUDE REGEX "SDL2main")
    target_link_libraries(ac_pc glad ${SDL2_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(ac_pc opengl32 winmm imm32 version setupapi)
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(ac_pc OpenGL::GL "-framework Cocoa" "-framework IOKit" pthread m)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(ac_pc OpenGL::GL dl pthread m)
endif()

set_target_properties(ac_pc PROPERTIES
    OUTPUT_NAME "AnimalCrossing"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Copy shader files to output directory (loaded at runtime, with embedded fallback)
add_custom_command(TARGET ac_pc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/default.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/default.frag"
        "${CMAKE_BINARY_DIR}/shaders/"
    COMMENT "Copying shader files to shaders/"
)

# Copy SDL2.dll next to executable so users don't need it on PATH
if(WIN32)
    find_file(SDL2_DLL_PATH SDL2.dll PATHS
        "${SDL2_MSYS2_DIR}/bin"
        "${SDL2_DOWNLOAD_DIR}/bin"
        "${SDL2_LOCAL_DIR}/bin"
        "$ENV{MINGW_PREFIX}/bin"
    )
    if(SDL2_DLL_PATH)
        add_custom_command(TARGET ac_pc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL2_DLL_PATH}" "${CMAKE_BINARY_DIR}/"
            COMMENT "Copying SDL2.dll to output directory/"
        )
    endif()
endif()

# Static link MinGW runtime (no libgcc_s_dw2-1.dll, libstdc++-6.dll, libwinpthread-1.dll)
# Strip version.o from winpthread to prevent its version resource from overriding app.rc
if(MINGW)
    find_file(WINPTHREAD_LIB libwinpthread.a PATHS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES} /mingw32/lib NO_DEFAULT_PATH)
    if(NOT WINPTHREAD_LIB)
        find_file(WINPTHREAD_LIB libwinpthread.a)
    endif()
    set(WINPTHREAD_STRIPPED "${CMAKE_CURRENT_BINARY_DIR}/libwinpthread_stripped.a")
    if(WINPTHREAD_LIB AND NOT EXISTS "${WINPTHREAD_STRIPPED}")
        file(COPY_FILE ${WINPTHREAD_LIB} ${WINPTHREAD_STRIPPED})
        execute_process(COMMAND ${CMAKE_AR} d "${WINPTHREAD_STRIPPED}" version.o)
        message(STATUS "Stripped version.o from winpthread: ${WINPTHREAD_STRIPPED}")
    endif()
    target_link_options(ac_pc PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive "${WINPTHREAD_STRIPPED}" -Wl,-Bdynamic,--no-whole-archive)
endif()

# Hide console window on Windows (GUI subsystem)
# Use -DPC_CONSOLE=ON to keep the console visible for debugging
if(WIN32 AND NOT PC_CONSOLE)
    target_link_options(ac_pc PRIVATE -mwindows)
endif()

# =============================================================================
# Install rules (cmake --install . --prefix <dest>)
# =============================================================================
install(TARGETS ac_pc RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/"
    DESTINATION shaders
    FILES_MATCHING PATTERN "*.vert" PATTERN "*.frag"
)
