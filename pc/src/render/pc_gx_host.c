/* pc_gx_host.c — AC-specific GX host hooks + emu64 diagnostics (Rainfall backend) */
#include "render/pc_renderer.h"
#include "pc_platform.h"
#include "pc_gx/rainfall_compat.h"

int pc_emu64_frame_cmds = 0;
int pc_emu64_frame_noop_cmds = 0;
int pc_emu64_frame_tri_cmds = 0;
int pc_emu64_frame_vtx_cmds = 0;
int pc_emu64_frame_dl_cmds = 0;
int pc_emu64_frame_cull_visible = 0;
int pc_emu64_frame_cull_rejected = 0;
int pc_gx_draw_call_count = 0;

float g_aspect_factor = 1.0f;
float g_aspect_offset = 0.0f;
int g_aspect_active = 0;

u16 s_tlut_first_word[16];

void pc_gx_flush_if_begin_complete(void) {
}

void pc_gx_tlut_set_native_le(unsigned int idx) {
    (void)idx;
}

void pc_heapMarkRenderThread(void) {
}

void pc_gx_begin_frame(void) {
    pc_emu64_frame_cmds = 0;
    pc_emu64_frame_noop_cmds = 0;
    pc_emu64_frame_tri_cmds = 0;
    pc_emu64_frame_vtx_cmds = 0;
    pc_emu64_frame_dl_cmds = 0;
    pc_emu64_frame_cull_visible = 0;
    pc_emu64_frame_cull_rejected = 0;
    pc_gx_draw_call_count = 0;
    g_pc_widescreen_stretch = 0;
    pc_renderer_begin_frame();
}

void pc_gx_restore_after_nes(void) {
    rfGXSetDirty(&g_gxSubmitState, RF_DIRTY_ALL);
}

void pc_gx_init(void) {
    /* GXInit + rfSetGPUFrameData are handled in pc_renderer_init(). */
}

void pc_gx_shutdown(void) {
    /* Rainfall shutdown is handled in pc_renderer_shutdown(). */
}
