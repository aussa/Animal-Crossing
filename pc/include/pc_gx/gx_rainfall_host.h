#ifndef AC_PC_GX_RAINFALL_HOST_H
#define AC_PC_GX_RAINFALL_HOST_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    u8 u8;
    u16 u16;
    u32 u32;
    s8 s8;
    s16 s16;
    s32 s32;
    f32 f32;
} GXWGPipe;

extern GXWGPipe GXWGFifo;

#ifdef __cplusplus
}
#endif

#endif
