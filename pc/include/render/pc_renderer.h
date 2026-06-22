/**
 * pc_renderer.h — thin host wrapper around Rainfall's renderer.
 */

#ifndef PC_RENDERER_H
#define PC_RENDERER_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int  pc_renderer_init(int windowW, int windowH, const char* title, float renderScale);
void pc_renderer_shutdown(void);
int  pc_renderer_is_initialized(void);

void pc_renderer_begin_frame(void);
void pc_renderer_end_frame(void);
int  pc_renderer_process_events(void);

void pc_renderer_update_window_size(void);
int  pc_renderer_get_window_w(void);
int  pc_renderer_get_window_h(void);

void* pc_renderer_get_window(void);

void pc_renderer_set_vsync(int enabled);

void pcGXGetClearColor(float* r, float* g, float* b, float* a);

#ifdef __cplusplus
}
#endif

#endif /* PC_RENDERER_H */
