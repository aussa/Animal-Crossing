#include "render/pc_renderer.h"
#include "pc_settings.h"

#include "pc_gx/gx_texture.h"
#include "pc_gx/gx_rainfall_host.h"
#include "pc_gx/rainfall_compat.h"
#include <dolphin/gx/GXManage.h>
#include <rainfall/render/rf/rf_renderer.h>
#ifdef GXWGFifo
#undef GXWGFifo
#endif
#include <rainfall/render/gx/gx_internal.h>
#include <rainfall/render/gpu/gpu.h>
#include <rainfall/window.h>

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

extern "C" void pc_platform_handle_event(const SDL_Event* event);

static void onEvent(void* eventPtr) {
    pc_platform_handle_event(static_cast<const SDL_Event*>(eventPtr));
}

extern "C" int pc_renderer_init(int windowW, int windowH, const char* title, float renderScale) {
    RFRendererConfig cfg = {};
    cfg.gameWidth = 640;
    cfg.gameHeight = 480;
    cfg.windowWidth = windowW > 0 ? windowW : 640;
    cfg.windowHeight = windowH > 0 ? windowH : 480;
    cfg.renderScale = renderScale > 0.0f ? renderScale : 1.0f;
    cfg.windowMode = RF_WINDOW_MODE_WINDOWED;
    cfg.windowTitle = title ? title : "Animal Crossing";
    cfg.getClearColor = pcGXGetClearColor;
    cfg.onEvent = onEvent;
    cfg.shaderCacheDir = "shader_cache";
#if defined(__APPLE__)
    /* Phase 1: OpenGL backend first — fewer moving parts while validating GX. */
    cfg.backendChoice = RF_BACKEND_CHOICE_OPENGL;
#else
    cfg.backendChoice = RF_BACKEND_CHOICE_AUTO;
#endif

    if (!rfRendererInit(&cfg)) {
        fprintf(stderr, "[PC] Rainfall renderer init failed\n");
        return 0;
    }

    rfRendererSetVSync(g_pc_settings.vsync ? true : false);
    rfSetGPUFrameData(rfRendererGetFrameData());
    (void)GXInit(nullptr, 0);
    return 1;
}

extern "C" void pc_renderer_shutdown(void) {
    if (!rfRendererIsInitialized()) return;
    rfRendererShutdown();
}

extern "C" int pc_renderer_is_initialized(void) {
    return rfRendererIsInitialized() ? 1 : 0;
}

extern "C" void pc_renderer_begin_frame(void) {
    if (!rfRendererIsInitialized()) return;
    rfRendererBeginFrame();
}

extern "C" void pc_renderer_end_frame(void) {
    if (!rfRendererIsInitialized()) return;
    rfRendererEndFrame();
}

extern "C" int pc_renderer_process_events(void) {
    if (!rfRendererIsInitialized()) return 1;
    return rfRendererProcessEvents() ? 1 : 0;
}

extern "C" void pc_renderer_update_window_size(void) {
    (void)rfRendererGetWindowWidth();
    (void)rfRendererGetWindowHeight();
}

extern "C" int pc_renderer_get_window_w(void) {
    return rfRendererIsInitialized() ? rfRendererGetWindowWidth() : 640;
}

extern "C" int pc_renderer_get_window_h(void) {
    return rfRendererIsInitialized() ? rfRendererGetWindowHeight() : 480;
}

extern "C" void* pc_renderer_get_window(void) {
    return (void*)rfRendererGetWindow().getNativeHandle();
}

extern "C" void pc_renderer_set_vsync(int enabled) {
    if (!rfRendererIsInitialized()) return;
    rfRendererSetVSync(enabled ? true : false);
}

extern "C" void pcGXGetClearColor(float* r, float* g, float* b, float* a) {
    if (!r || !g || !b || !a) return;
    *r = g_gxSubmitState.clearColor.r / 255.0f;
    *g = g_gxSubmitState.clearColor.g / 255.0f;
    *b = g_gxSubmitState.clearColor.b / 255.0f;
    *a = g_gxSubmitState.clearColor.a / 255.0f;
}

bool pcMirrorModeIsActive(void) {
    return false;
}

extern "C" float pcRendererReadDepth(u16 x, u16 y) {
    (void)x;
    (void)y;
    return 1.0f;
}
