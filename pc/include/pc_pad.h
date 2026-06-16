#ifndef PC_PAD_H
#define PC_PAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Controller hotplug hooks, driven from the SDL event loop. They keep the
 * active GameController in sync when a pad is connected, disconnected, or
 * re-enumerated after a system sleep/resume. */

/* SDL_CONTROLLERDEVICEADDED: `device_index` is a joystick device index. */
void pc_pad_device_added(int device_index);

/* SDL_CONTROLLERDEVICEREMOVED: `instance_id` is a joystick instance id. */
void pc_pad_device_removed(int instance_id);

#ifdef __cplusplus
}
#endif

#endif /* PC_PAD_H */
