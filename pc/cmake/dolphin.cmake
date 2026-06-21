# =============================================================================
# Animal Crossing PC Port - PC Layer Source Files
# =============================================================================

set(PC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pc_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/pc_gx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/pc_gx_tev.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/pc_gx_texture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/pc_gbi_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/pc_os.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utility/pc_dvd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input/pc_pad.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input/pc_typing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input/pc_mouse.c
    ${DECOMP_ROOT}/src/static/dolphin/pad/Padclamp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/pc_card.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/pc_vi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/pc_audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/pc_aram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utility/pc_misc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/pc_mtx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/pc_m_card.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/pc_save_bswap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/menus/pc_model_viewer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/pc_texture_pack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/pc_settings.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/menus/pc_pause_menu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/menus/pc_settings_menu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/menus/pc_menu_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/menus/pc_text_draw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/pc_keybindings.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utility/pc_nes_fixnes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/pc_stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/pc_stubs_cpp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utility/pc_assets.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utility/pc_disc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/pc_bc7.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/bc7decomp/bc7decomp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utility/pc_japan_msg.c
)

# =============================================================================
# PC Port Warnings Configuration
# =============================================================================
# Enable warnings to catch bugs in code we control
set(PC_C_SOURCES ${PC_SOURCES})
list(FILTER PC_C_SOURCES EXCLUDE REGEX "\\.cpp$")
set(PC_CXX_SOURCES ${PC_SOURCES})
list(FILTER PC_CXX_SOURCES INCLUDE REGEX "\\.cpp$")

# Note: -Wno-comment/-Wno-unused-function/-Wno-builtin-declaration-mismatch suppress
# warnings from decomp headers (types.h, gfxprint.h, OSRtc.h, libultra.h) that we can't modify.
set(PC_WARN_SUPPRESS "-Wno-unused-parameter;-Wno-sign-compare;-Wno-comment;-Wno-unused-function")
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    list(APPEND PC_WARN_SUPPRESS "-Wno-builtin-declaration-mismatch")
endif()

set_source_files_properties(${PC_C_SOURCES}
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;${PC_WARN_SUPPRESS}"
)
set_source_files_properties(${PC_CXX_SOURCES}
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;${PC_WARN_SUPPRESS};-fpermissive"
)
