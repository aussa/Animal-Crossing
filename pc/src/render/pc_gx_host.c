/* pc_gx_host.c — AC-specific GX host hooks + emu64 diagnostics (Rainfall backend) */
#include "render/pc_renderer.h"
#include "pc_platform.h"
#include "pc_gx/rainfall_compat.h"
#include <rainfall/render/gx/gx.h>

extern void rfGXSetTlutNativeEndian(u32 id, GXBool native);
extern void rfGXFlushImmediateIfComplete(void);

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

static int pc_gx_tlut_force_be(void) {
    static int init = 0;
    static int force_be = 0;
    if (!init) {
        const char* mode = getenv("AC_GX_TLUT_ENDIAN");
        if (!mode) mode = getenv("PC_GX_TLUT_ENDIAN");
        if (mode && (mode[0] == 'b' || mode[0] == 'B')) {
            force_be = 1;
        }
        init = 1;
    }
    return force_be;
}

static void pc_gx_update_aspect(void) {
    float gc_aspect = (float)PC_GC_WIDTH / (float)PC_GC_HEIGHT;
    float win_aspect = (float)g_pc_window_w / (float)g_pc_window_h;
    if (win_aspect > gc_aspect + 0.01f) {
        g_aspect_factor = gc_aspect / win_aspect;
        g_aspect_offset = (1.0f - g_aspect_factor) / 2.0f * (float)PC_GC_WIDTH;
        g_aspect_active = 1;
    } else {
        g_aspect_factor = 1.0f;
        g_aspect_offset = 0.0f;
        g_aspect_active = 0;
    }
}

void pc_gx_flush_if_begin_complete(void) {
    rfGXFlushImmediateIfComplete();
}

void ac_gx_patch_projection(f32 mtx[4][4], GXProjectionType type) {
#ifdef PC_ENHANCEMENTS
    if (g_pc_widescreen_stretch == 0 ||
        (g_pc_widescreen_stretch == 2 && type == GX_ORTHOGRAPHIC)) {
        if (g_aspect_active) {
            mtx[0][0] *= g_aspect_factor;
        }
    }
#else
    (void)type;
    (void)mtx;
#endif
}

void pc_gx_tlut_set_native_le(unsigned int idx) {
    if (idx < 16) {
        rfGXSetTlutNativeEndian(idx, pc_gx_tlut_force_be() ? GX_FALSE : GX_TRUE);
    }
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
    pc_gx_update_aspect();
    GX_FlushVertices_resetFrame();
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
