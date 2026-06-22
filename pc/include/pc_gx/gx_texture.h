/**
 * PC GX texture/palette types and host hooks used by gx_pc.cpp.
 * Texture decoding and GPU upload are handled by Rainfall.
 */

#ifndef PC_GX_TEXTURE_H
#define PC_GX_TEXTURE_H

#include "dolphin/gx/GXEnum.h"
#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GX_MAX_PALETTE_ENTRIES 16384
#define GX_MAX_PALETTES 32

typedef struct GXPaletteEntry {
    bool valid;
    u32 format;
    u16 numEntries;
    u8 rgba[GX_MAX_PALETTE_ENTRIES][4];
} GXPaletteEntry;

extern GXPaletteEntry g_palettes[GX_MAX_PALETTES];

void pcTextureInvalidateRange(uintptr_t start, uintptr_t end);
void pcGcAddrMapClearRange(uintptr_t start, uintptr_t end);

const void* rfGXGetTlutSourcePtr(u32 id);
GXBool rfGXGetTlutNativeEndian(u32 id);

#ifdef __cplusplus
}
#endif

#endif /* PC_GX_TEXTURE_H */
