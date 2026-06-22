# =============================================================================
# Animal Crossing PC Port - Rainfall (GX → SDL3 GPU / OpenGL)
# =============================================================================

set(RAINFALL_INCLUDE_GX_API ON CACHE BOOL "" FORCE)
set(RAINFALL_HOST_PROVIDES_GX_TYPES ON CACHE BOOL "" FORCE)
set(RAINFALL_HOST_GX_INCLUDE_PATH "\"dolphin/gx/GXEnum.h\"" CACHE STRING "" FORCE)
set(RAINFALL_INCLUDE_AUDIO OFF CACHE BOOL "" FORCE)
set(RAINFALL_INCLUDE_MOVIE OFF CACHE BOOL "" FORCE)
set(RAINFALL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RAINFALL_BUILD_SIPHON OFF CACHE BOOL "" FORCE)
set(RAINFALL_DEV_MODE OFF CACHE BOOL "" FORCE)
set(RAINFALL_DEV_IMGUI OFF CACHE BOOL "" FORCE)

if(APPLE)
    set(SDLSHADERCROSS_DXC OFF CACHE BOOL "" FORCE)
endif()

find_package(SDL3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
set(HAS_OPENGL TRUE)

set(_ac_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/external/rainfall"
    "${CMAKE_CURRENT_BINARY_DIR}/rainfall"
)

set(BUILD_SHARED_LIBS "${_ac_saved_build_shared_libs}" CACHE BOOL "" FORCE)
unset(_ac_saved_build_shared_libs)

# Host GX types + PC overrides must shadow game headers for Rainfall.
target_include_directories(rainfall PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${DECOMP_ROOT}/include"
)
target_compile_definitions(rainfall PUBLIC PLATFORM_PC=1 TARGET_PC=1)

set(HAS_SDL3 TRUE)
if(HAS_SHADERCROSS)
    set(HAS_SDL3_GPU TRUE)
else()
    set(HAS_SDL3_GPU FALSE)
endif()

message(STATUS "Rainfall: HAS_SDL3=${HAS_SDL3} HAS_SDL3_GPU=${HAS_SDL3_GPU} HAS_OPENGL=${HAS_OPENGL}")
