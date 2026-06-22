/* pc_platform_events.c — SDL3 input/window events for the Rainfall window */
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_typing.h"
#include "pc_pause_menu.h"
#include "pc_pad.h"
#include "pc_mouse.h"
#include "render/pc_renderer.h"

#include <stdio.h>

extern void pc_speedhack_toggle(void);

void pc_platform_handle_event(const SDL_Event* event) {
    if (!event) return;

    switch (event->type) {
        case SDL_EVENT_QUIT:
            g_pc_running = 0;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            pc_platform_update_window_size();
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            g_pc_minimized = 1;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            g_pc_minimized = 0;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            pc_renderer_set_vsync(0);
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            pc_renderer_set_vsync(g_pc_settings.vsync);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            pc_pad_device_added((int)event->gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            pc_pad_device_removed((int)event->gdevice.which);
            break;
#ifdef MOUSE_INPUT
        case SDL_EVENT_MOUSE_WHEEL:
            g_mouse_wheel_delta += event->wheel.y;
            break;
#endif
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_F3 && !event->key.repeat) {
                pc_speedhack_toggle();
                break;
            }
            if (event->key.key == SDLK_F4 && !event->key.repeat) {
                g_pc_fast_forward ^= 1;
                printf("[PC] Fast forward %s (2x)\n", g_pc_fast_forward ? "ON" : "OFF");
            }
            if (event->key.key == SDLK_ESCAPE && !event->key.repeat) {
                if (g_pc_paused) {
                    pc_pause_menu_handle_event(event);
                } else {
                    pc_pause_menu_toggle();
                }
                break;
            }
            if (g_pc_paused) {
                pc_pause_menu_handle_event(event);
                break;
            }
            pc_typing_handle_event(event);
            break;
        case SDL_EVENT_TEXT_INPUT:
            if (g_pc_paused) break;
            pc_typing_handle_event(event);
            break;
        default:
            break;
    }
}

int pc_platform_poll_events(void) {
    pc_typing_update();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        pc_platform_handle_event(&event);
        if (!g_pc_running) return 0;
    }

    pc_mouse_update();
    return g_pc_running ? 1 : 0;
}

void pc_platform_update_window_size(void) {
    if (g_pc_window) {
        SDL_GetWindowSizeInPixels(g_pc_window, &g_pc_window_w, &g_pc_window_h);
    } else {
        g_pc_window_w = pc_renderer_get_window_w();
        g_pc_window_h = pc_renderer_get_window_h();
    }
    if (g_pc_window_w <= 0) g_pc_window_w = PC_SCREEN_WIDTH;
    if (g_pc_window_h <= 0) g_pc_window_h = PC_SCREEN_HEIGHT;
}

void pc_platform_swap_buffers(void) {
    if (g_pc_minimized) {
        SDL_Delay(16);
        return;
    }
    pc_renderer_end_frame();
}
