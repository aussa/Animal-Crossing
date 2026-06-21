/* pc_pad.c - GC controller input via SDL gamepad + keyboard */
#include "pc_platform.h"
#include "pc_typing.h"
#include "pc_keybindings.h"
#include "pc_settings.h"
#include <dolphin/pad.h>
#include <math.h>

/* analog stick constants */
#define STICK_MAGNITUDE     80
#define TRIGGER_THRESHOLD   100
#define RUMBLE_DURATION_MS  200

/* Radial-deadzone calibration (see src/static/dolphin/pad/Padclamp.c):
 * the game's own PADClamp applies an inner deadzone of 15 and expects full
 * deflection around 80 (the keyboard path uses STICK_MAGNITUDE = 80). The old
 * gamepad path mapped to +/-127, overshooting so the stick saturated at ~60%
 * travel. We map the live region onto [FLOOR .. RIM] instead. FLOOR = 16 sits
 * just above PADClamp's min (15) so crossing our deadzone yields motion of 1
 * post-clamp - a smooth onset without stacking a second dead band. */
#define PAD_STICK_FLOOR     16
#define PAD_STICK_RIM       80
#define PAD_STICK_OUTER     0.95f   /* saturation: reach max before the physical rim */
#define SDL_AXIS_MAX        32767.0f

extern int g_pc_verbose;

static SDL_GameController* g_controller = NULL;
/* Instance id of the open controller, used to match SDL_CONTROLLERDEVICEREMOVED
 * events (whose `which` is an instance id, not a device index). -1 = none. */
static SDL_JoystickID g_controller_id = -1;

/* Open the game controller at joystick device index `idx`, unless we already
 * hold one (the first pad wins; extras are ignored). Records its instance id
 * so a later REMOVED event can be matched to it. */
static void pad_open(int idx) {
    if (g_controller) return;
    if (!SDL_IsGameController(idx)) return;
    SDL_GameController* c = SDL_GameControllerOpen(idx);
    if (!c) return;
    g_controller = c;
    SDL_Joystick* js = SDL_GameControllerGetJoystick(c);
    g_controller_id = js ? SDL_JoystickInstanceID(js) : -1;
    if (g_pc_verbose) {
        printf("[PAD] opened controller idx=%d instance=%d name=%s\n",
               idx, (int)g_controller_id, SDL_GameControllerName(c));
    }
}

/* Scan every joystick and open the first game controller found. */
static void pad_open_first(void) {
    int n = SDL_NumJoysticks();
    for (int i = 0; i < n && !g_controller; i++) {
        pad_open(i);
    }
}

static void pad_close(void) {
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
        if (g_pc_verbose) printf("[PAD] closed controller instance=%d\n", (int)g_controller_id);
    }
    g_controller_id = -1;
}

/* Radial deadzone: maps a raw SDL stick (x,y in [-32768,32767]) to GC stick
 * units, preserving direction. Returns TRUE and writes *ox,*oy only when the
 * stick is OUTSIDE the deadzone; returns FALSE (leaving *ox,*oy untouched)
 * when inside, so a keyboard-driven stick value is preserved. dz_inner is the
 * deadzone fraction [0..1]; curve is the response exponent (1.0 = linear).
 * Y is inverted to match the GC's Y-up convention. */
static BOOL apply_radial_deadzone(s16 x, s16 y, f32 dz_inner, f32 curve, s8* ox, s8* oy) {
    /* Cheap inscribed-box early-out: when both axes are within the box that
     * fits entirely inside the deadzone circle, the point is guaranteed inside
     * -> no motion, skip the sqrtf. 0.70710678 (1/sqrt2) keeps the box
     * inscribed so its corners never poke outside the circle. */
    f32 box = dz_inner * SDL_AXIS_MAX * 0.70710678f;
    if ((f32)abs(x) <= box && (f32)abs(y) <= box) {
        return FALSE;
    }

    f32 fx = (f32)x * (1.0f / SDL_AXIS_MAX);
    f32 fy = (f32)y * (1.0f / SDL_AXIS_MAX);
    f32 mag = sqrtf(fx * fx + fy * fy);

    /* Guard the divide below: dead-centre (mag==0) or a deadzone of 0. */
    if (mag <= dz_inner || mag < 1e-6f) {
        return FALSE;
    }

    /* Rescale the live region [dz_inner .. OUTER] onto [0 .. 1]. */
    f32 range = PAD_STICK_OUTER - dz_inner;
    if (range < 1e-6f) range = 1e-6f;   /* defensive: never divide by ~0 */
    f32 t = (mag - dz_inner) / range;
    if (t > 1.0f) t = 1.0f;

    /* Response curve: t stays in [0,1] and monotonic for curve>0. Skip the
     * powf for the default linear case so it costs exactly what v1 did. */
    if (curve != 1.0f) t = powf(t, curve);

    /* Map onto GC units with floor compensation for PADClamp's min. */
    f32 out = (f32)PAD_STICK_FLOOR + t * (f32)(PAD_STICK_RIM - PAD_STICK_FLOOR);

    /* Direction via the unit vector; clamp as float-rounding insurance. */
    f32 gx = (fx / mag) * out;
    f32 gy = (fy / mag) * out;
    if (gx > 127.0f) gx = 127.0f; else if (gx < -128.0f) gx = -128.0f;
    if (gy > 127.0f) gy = 127.0f; else if (gy < -128.0f) gy = -128.0f;

    *ox = (s8)gx;
    *oy = (s8)(-gy);   /* GC stick is Y-up; SDL is Y-down */
    return TRUE;
}

BOOL PADInit(void) {
    /* Make sure SDL delivers controller hotplug events to our poll loop so
     * pads that (re)appear after a system sleep/resume get picked up. */
    SDL_GameControllerEventState(SDL_ENABLE);
    pad_open_first();
    return TRUE;
}

/* SDL_CONTROLLERDEVICEADDED hook (event.cdevice.which is a device index).
 * Fires when a pad is plugged in or re-enumerates after a sleep/resume. */
void pc_pad_device_added(int device_index) {
    if (g_pc_verbose) printf("[PAD] event DEVICE_ADDED index=%d\n", device_index);
    pad_open(device_index);
}

/* SDL_CONTROLLERDEVICEREMOVED hook (event.cdevice.which is an instance id).
 * If our active pad went away, drop it and try to fall back to any other
 * controller still connected (e.g. dock vs handheld). */
void pc_pad_device_removed(int instance_id) {
    if (g_pc_verbose) {
        printf("[PAD] event DEVICE_REMOVED instance=%d (ours=%d)\n",
               instance_id, (int)g_controller_id);
    }
    if (g_controller && instance_id == g_controller_id) {
        pad_close();
        pad_open_first();
    }
}

u32 PADRead(PADStatus* status) {
    memset(status, 0, sizeof(PADStatus) * 4);

    const u8* keys = SDL_GetKeyboardState(NULL);
    u32 mouse = SDL_GetMouseState(NULL, NULL);
    u16 buttons = 0;
    s8 stickX = 0, stickY = 0;
    s8 cstickX = 0, cstickY = 0;

    /* Suppress keyboard-to-button mapping when typing into the in-game text editor */
    if (!(g_pc_typing_mode && g_pc_editor_active)) {
        /* helper: check if a PCInputCode is currently pressed */
        #define INPUT_PRESSED(code) \
            (((code) & PC_INPUT_MOUSE_BIT) \
                ? (mouse & SDL_BUTTON((code) & 0xFF)) \
                : keys[(SDL_Scancode)(code)])

        /* buttons (from keybindings.ini) */
        PCKeybindings* kb = &g_pc_keybindings;
        if (INPUT_PRESSED(kb->a))     buttons |= PAD_BUTTON_A;
        if (INPUT_PRESSED(kb->b))     buttons |= PAD_BUTTON_B;
        if (INPUT_PRESSED(kb->x))     buttons |= PAD_BUTTON_X;
        if (INPUT_PRESSED(kb->y))     buttons |= PAD_BUTTON_Y;
        if (INPUT_PRESSED(kb->start)) buttons |= PAD_BUTTON_START;
        if (INPUT_PRESSED(kb->z))     buttons |= PAD_TRIGGER_Z;
        if (INPUT_PRESSED(kb->l))     buttons |= PAD_TRIGGER_L;
        if (INPUT_PRESSED(kb->r))     buttons |= PAD_TRIGGER_R;

        /* main stick */
        if (INPUT_PRESSED(kb->stick_up))    stickY += STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->stick_down))  stickY -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->stick_left))  stickX -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->stick_right)) stickX += STICK_MAGNITUDE;

        /* C-stick */
        if (INPUT_PRESSED(kb->cstick_up))    cstickY += STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->cstick_down))  cstickY -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->cstick_left))  cstickX -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->cstick_right)) cstickX += STICK_MAGNITUDE;

        /* D-pad */
        if (INPUT_PRESSED(kb->dpad_up))    buttons |= PAD_BUTTON_UP;
        if (INPUT_PRESSED(kb->dpad_down))  buttons |= PAD_BUTTON_DOWN;
        if (INPUT_PRESSED(kb->dpad_left))  buttons |= PAD_BUTTON_LEFT;
        if (INPUT_PRESSED(kb->dpad_right)) buttons |= PAD_BUTTON_RIGHT;

        #undef INPUT_PRESSED
    }

    /* Hotplug safety net. The SDL event loop drives (re)connection via
     * pc_pad_device_added/removed, but we re-check here every frame so a
     * dropped pad is recovered even if an ADDED event was missed on some
     * resume paths: detach a stale handle, then (re)acquire whatever's there. */
    if (g_controller && !SDL_GameControllerGetAttached(g_controller)) {
        pad_close();
    }
    if (!g_controller) {
        pad_open_first();
    }
    if (g_controller) {
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_A)) buttons |= PAD_BUTTON_A;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_B)) buttons |= PAD_BUTTON_B;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_X)) buttons |= PAD_BUTTON_X;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_Y)) buttons |= PAD_BUTTON_Y;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_START)) buttons |= PAD_BUTTON_START;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_BACK))  buttons |= PAD_BUTTON_START;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  buttons |= PAD_TRIGGER_L;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) buttons |= PAD_TRIGGER_Z;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_UP))    buttons |= PAD_BUTTON_UP;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  buttons |= PAD_BUTTON_DOWN;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  buttons |= PAD_BUTTON_LEFT;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= PAD_BUTTON_RIGHT;

        /* Per-stick deadzone + shared response curve, computed once per read so
         * a live settings-menu change applies next frame with no restart. */
        f32 dz_main   = (f32)g_pc_settings.controller_deadzone * 0.01f;
        f32 dz_cstick = (f32)g_pc_settings.controller_deadzone_cstick * 0.01f;
        f32 curve     = (f32)g_pc_settings.controller_response_curve * 0.01f;

        s16 lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
        s16 ly = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
        apply_radial_deadzone(lx, ly, dz_main, curve, &stickX, &stickY);

        s16 rx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
        s16 ry = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);
        apply_radial_deadzone(rx, ry, dz_cstick, curve, &cstickX, &cstickY);

        u8 lt = (u8)(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
        u8 rt = (u8)(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);
        if (lt > TRIGGER_THRESHOLD) buttons |= PAD_TRIGGER_L;
        if (rt > TRIGGER_THRESHOLD) buttons |= PAD_TRIGGER_R;
        status[0].triggerLeft = lt;
        status[0].triggerRight = rt;
    }

    status[0].button = buttons;
    status[0].stickX = stickX;
    status[0].stickY = stickY;
    status[0].substickX = cstickX;
    status[0].substickY = cstickY;
    status[0].err = 0; /* PAD_ERR_NONE */

    return PAD_CHAN0_BIT; /* Controller 1 connected */
}

void PADControlMotor(s32 chan, u32 command) {
    if (g_controller && chan == 0) {
        u16 intensity = (command == 1) ? 0xFFFF : 0;
        SDL_GameControllerRumble(g_controller, intensity, intensity, RUMBLE_DURATION_MS);
    }
}

void PADControlAllMotors(const u32* commands) {
    PADControlMotor(0, commands[0]);
}

void PADCleanup(void) {
    pad_close();
}

BOOL PADReset(u32 mask) { (void)mask; return TRUE; }
BOOL PADRecalibrate(u32 mask) { (void)mask; return TRUE; }
BOOL PADSync(void) { return TRUE; }
void PADSetSpec(u32 spec) { (void)spec; }
void PADSetAnalogMode(u32 mode) { (void)mode; }
/* PADClamp compiled from decomp: src/static/dolphin/pad/Padclamp.c */
BOOL PADGetType(s32 chan, u32* type) { if (type) *type = 0x09000000; return TRUE; }
