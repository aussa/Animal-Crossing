#ifndef PC_SETTINGS_H
#define PC_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int window_width;
    int window_height;
    int fullscreen;       /* 0=windowed, 1=fullscreen, 2=borderless */
    int vsync;            /* 0=off, 1=on */
    int max_fps;          /* 0=cap, otherwise frame limiter target */
    int msaa;             /* 0=off, 2/4/8=samples */
    int preload_textures; /* 0=off (load on demand), 1=on (load all at startup), 2=on + cache file */
    int disable_resetti;  /* 0=normal (Resetti appears on reset), 1=disable reset penalty */
    int nes_aspect;       /* NES emulator aspect: 0=fullscreen stretch, 1=4:3 pillarbox (default) */
    int master_volume;    /* Applied at the PC audio output, 0-100 (default 100) */
    int controller_deadzone;        /* Main stick radial deadzone, percent of full range, 0-PC_DEADZONE_MAX (default 15) */
    int controller_deadzone_cstick; /* C-stick radial deadzone, percent, 0-PC_DEADZONE_MAX (default 15) */
    int controller_response_curve;  /* Stick response curve exponent x100, PC_CURVE_MIN-PC_CURVE_MAX (100 = linear) */
} PCSettings;

#define PC_MAX_FPS_CAP 960
#define PC_DEADZONE_MAX 40    /* Upper bound for controller deadzones (percent) */
#define PC_CURVE_MIN    50    /* 0.50x - steeper/twitchier */
#define PC_CURVE_MAX    300   /* 3.00x - flatter/finer near centre */

extern PCSettings g_pc_settings;

void pc_settings_load(void);
void pc_settings_save(void);
void pc_settings_apply(void);
void pc_settings_cycle_resolution(int* width, int* height, int dir);

#ifdef __cplusplus
}
#endif

#endif /* PC_SETTINGS_H */
