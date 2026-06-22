/**
 * Animal Crossing PC force-include / platform shim for Rainfall.
 *
 * Twilight Princess ships its own rainfall/platform/pc.h with JSystem-specific
 * helpers (be_val.h, J3D tags, etc.). AC uses a slimmer header that is safe
 * to include from both C and C++ translation units.
 */

#ifndef AC_RAINFALL_PLATFORM_PC_H
#define AC_RAINFALL_PLATFORM_PC_H

#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "dolphin/types.h"
#include "dolphin/os.h"
#include "dolphin/mtx.h"
#include "dolphin/gx.h"

#include "rainfall/platform/macros.h"

#ifdef MOUSE_INPUT
#include "input/pc_mouse.h"
#endif

#ifndef ASSERT
#define ASSERT(x) ((void)0)
#endif
#ifndef ASSERTMSG
#define ASSERTMSG(x, msg) ((void)0)
#endif
#ifndef JUT_ASSERT
#define JUT_ASSERT(...) ((void)0)
#endif
#ifndef JUT_ASSERT_MSG
#define JUT_ASSERT_MSG(...) ((void)0)
#endif
#ifndef JUT_ASSERT_MSG_F
#define JUT_ASSERT_MSG_F(...) ((void)0)
#endif
#ifndef J3D_PANIC
#define J3D_PANIC(...) (void)0
#endif
#ifndef JUT_PANIC
#define JUT_PANIC(...)
#endif
#ifndef JUT_PANIC_F
#define JUT_PANIC_F(...)
#endif
#ifndef JUT_WARN
#define JUT_WARN(...) (void)0
#endif

#ifdef _WIN32
#undef IN
#undef OUT
#undef OPTIONAL
#undef near
#undef far
#endif

#ifdef __cplusplus
extern "C" {
#endif
u32 pcRegisterEmbeddedTexAddr(const void* addr);
void GXSetArrayNativeEndian_PC(bool native);
void _strip(float x);
int __abs(int x);
#ifdef __cplusplus
}
#endif

#endif /* AC_RAINFALL_PLATFORM_PC_H */
