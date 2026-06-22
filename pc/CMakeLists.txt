# =============================================================================
# Animal Crossing PC Port - CMake Build Configuration
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(ac_pc C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Options
option(PC_CONSOLE "Show console window (disables -mwindows, useful for debugging)" OFF)
option(AC_USE_RAINFALL "Use Rainfall renderer instead of legacy pc_gx" OFF)

# Rainfall bisect toggles (only meaningful with AC_USE_RAINFALL=ON)
option(AC_RAINFALL_BISECT_NO_FLUSH "Bisect: stub pc_gx_flush_if_begin_complete" OFF)
option(AC_RAINFALL_BISECT_NO_FORCE_INCLUDE "Bisect: disable force-include of rainfall/platform/pc.h" OFF)
option(AC_RAINFALL_BISECT_NO_TEXTURE_BIND "Bisect: skip pc_texture_pack_bind_rainfall" OFF)
option(AC_RAINFALL_BISECT_LEGACY_GXWGFIFO "Bisect: use decomp GXWGFifo address macro" OFF)

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
if(AC_USE_RAINFALL)
    include(cmake/rainfall.cmake)
endif()
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
)

if(AC_USE_RAINFALL)
    target_include_directories(ac_pc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/pc_gx
        ${RAINFALL_SOURCE_DIR}/include
    )
    target_link_libraries(ac_pc PRIVATE rainfall)
endif()

target_link_libraries(ac_pc PRIVATE glad SDL3::SDL3)

if(WIN32)
    target_link_libraries(ac_pc PRIVATE opengl32 winmm imm32 version setupapi)
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(ac_pc PRIVATE OpenGL::GL "-framework Cocoa" "-framework IOKit" pthread m)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(ac_pc PRIVATE OpenGL::GL dl pthread m)
endif()

if(AC_USE_RAINFALL)
    set(AC_FORCE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include/rainfall/platform/pc.h")
    if(NOT AC_RAINFALL_BISECT_NO_FORCE_INCLUDE)
    set_source_files_properties(
        ${GAME_C_SOURCES} ${GAME_CXX_SOURCES}
        PROPERTIES
            COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/FI${AC_FORCE_INCLUDE}>;$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include;${AC_FORCE_INCLUDE}>"
    )
    endif()
endif()

set_target_properties(ac_pc PROPERTIES
    OUTPUT_NAME "AnimalCrossing"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Copy legacy pc_gx shader files
if(NOT AC_USE_RAINFALL)
add_custom_command(TARGET ac_pc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/default.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/default.frag"
        "${CMAKE_BINARY_DIR}/shaders/"
    COMMENT "Copying shader files to shaders/"
)
endif()

# Static link MinGW runtime
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
