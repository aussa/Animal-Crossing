#include "pc_gx/gx_texture.h"
#include "pc_gx/gx_rainfall_host.h"
#include "pc_gx/rainfall_compat.h"
#include <rainfall/rainfall.h>
#include <rainfall/render/texture/texture_cache.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define g_gxState g_gxSubmitState

static inline u8 pal_expand3to8(u8 val) { return (u8)((val << 5) | (val << 2) | (val >> 1)); }
static inline u8 pal_expand4to8(u8 val) { return (u8)((val << 4) | val); }
static inline u8 pal_expand5to8(u8 val) { return (u8)((val << 3) | (val >> 2)); }

static void GX_DecodePaletteEntry(const u8* src, GXTlutFmt format, u8* rgba) {
    switch (format) {
        case GX_TL_IA8:
            rgba[0] = rgba[1] = rgba[2] = src[0];
            rgba[3] = src[1];
            break;
        case GX_TL_RGB565: {
            u16 pixel = (u16)((src[0] << 8) | src[1]);
            rgba[0] = pal_expand5to8((u8)((pixel >> 11) & 0x1F));
            rgba[1] = pal_expand5to8((u8)((pixel >> 5) & 0x3F));
            rgba[2] = pal_expand5to8((u8)(pixel & 0x1F));
            rgba[3] = 255;
            break;
        }
        case GX_TL_RGB5A3: {
            u16 pixel = (u16)((src[0] << 8) | src[1]);
            if (pixel & 0x8000) {
                rgba[0] = pal_expand5to8((u8)((pixel >> 10) & 0x1F));
                rgba[1] = pal_expand5to8((u8)((pixel >> 5) & 0x1F));
                rgba[2] = pal_expand5to8((u8)(pixel & 0x1F));
                rgba[3] = 255;
            } else {
                rgba[3] = pal_expand3to8((u8)((pixel >> 12) & 0x7));
                rgba[0] = pal_expand4to8((u8)((pixel >> 8) & 0xF));
                rgba[1] = pal_expand4to8((u8)((pixel >> 4) & 0xF));
                rgba[2] = pal_expand4to8((u8)(pixel & 0xF));
            }
            break;
        }
        default:
            rgba[0] = rgba[1] = rgba[2] = 128;
            rgba[3] = 255;
            break;
    }
}

void GX_FlushVertices_resetFrame(void) {
    g_rfTextureCache.flushDeferred();
}

extern "C" void pcTextureInvalidateRange(uintptr_t start, uintptr_t end) {
    g_rfTextureCache.invalidateRange(start, end);
}

extern "C" void pcGcAddrMapClearRange(uintptr_t start, uintptr_t end) {
    (void)start;
    (void)end;
}

extern "C" u32 pcRegisterEmbeddedTexAddr(const void* addr) {
    (void)addr;
    return 0;
}

#ifdef __cplusplus
}
#endif

bool g_inItem3DDrawMode = false;

#ifdef __cplusplus
extern "C" {
#endif

bool g_rfSkipDLTevColorReg1 = false;

extern "C" void rfDLTexImageAddr(u32 texIdx, u32 bpData) {
    if (texIdx >= 8) {
        return;
    }
    g_gxState.textures[texIdx].data = (void*)((uintptr_t)bpData << 5);
}

extern "C" void rfDLTexImageDims(u32 texIdx, u16 width, u16 height, u8 format) {
    if (texIdx >= 8) {
        return;
    }
    g_gxState.textures[texIdx].width = width;
    g_gxState.textures[texIdx].height = height;
    g_gxState.textures[texIdx].format = (GXTexFmt)format;
    g_gxState.textures[texIdx].valid = GX_TRUE;
    rfGXSetDirty(&g_gxState, RF_DIRTY_TEXTURE);
}

#ifdef __cplusplus
}
#endif
