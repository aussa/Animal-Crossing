#include "pc_gx/gx_texture.h"
#include "pc_gx/gx_rainfall_host.h"
#include "pc_gx/rainfall_compat.h"
#include "pc_texture_pack.h"
#include <rainfall/rainfall.h>
#include <rainfall/render/gx/gx_texture.h>
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

static int gc_texture_data_size(int w, int h, u8 fmt) {
    int bpp;
    switch (fmt) {
        case GX_TF_I4: bpp = 4; break;
        case GX_TF_I8: bpp = 8; break;
        case GX_TF_IA4: bpp = 8; break;
        case GX_TF_IA8: bpp = 16; break;
        case GX_TF_RGB565: bpp = 16; break;
        case GX_TF_RGB5A3: bpp = 16; break;
        case GX_TF_RGBA8: bpp = 32; break;
        case GX_TF_CMPR: bpp = 4; break;
        case GX_TF_C4: bpp = 4; break;
        case GX_TF_C8: bpp = 8; break;
        case GX_TF_C14X2: bpp = 16; break;
        default: bpp = 16; break;
    }
    return (w * h * bpp) / 8;
}

static u32 find_tlut_for_image(const void* imagePtr, u8 format) {
    if (!rainfallIsTexFormatPaletted(format)) {
        return 0;
    }
    for (u32 i = 0; i < GX_MAX_TEXMAP; i++) {
        if (g_gxSubmitState.textures[i].data == imagePtr) {
            return g_gxSubmitState.textures[i].tlutName;
        }
    }
    return 0;
}

static SDL_GPUTexture* acTexPackOverride(const void* imagePtr, u16 width, u16 height,
                                         u8 format, u16* outW, u16* outH, u32* outMipLevels) {
    if (!pc_texture_pack_active() || !imagePtr || width == 0 || height == 0) {
        return nullptr;
    }

    int data_size = gc_texture_data_size(width, height, format);
    const void* tp_tlut = nullptr;
    int tp_tlut_entries = 0;
    int tp_tlut_is_be = 1;

    u32 tlutIdx = find_tlut_for_image(imagePtr, format);
    if (tlutIdx < GX_MAX_PALETTES && g_palettes[tlutIdx].valid) {
        tp_tlut = rfGXGetTlutSourcePtr(tlutIdx);
        tp_tlut_entries = g_palettes[tlutIdx].numEntries;
        tp_tlut_is_be = rfGXGetTlutNativeEndian(tlutIdx) ? 0 : 1;
    }

    int hd_w = 0;
    int hd_h = 0;
    GLuint hd_tex = pc_texture_pack_lookup(imagePtr, data_size, width, height, format,
                                           tp_tlut, tp_tlut_entries, tp_tlut_is_be,
                                           &hd_w, &hd_h);
    if (!hd_tex) {
        return nullptr;
    }

    if (outW) *outW = (u16)hd_w;
    if (outH) *outH = (u16)hd_h;
    if (outMipLevels) *outMipLevels = 1;
    return reinterpret_cast<SDL_GPUTexture*>(static_cast<uintptr_t>(hd_tex));
}

extern "C" void pc_texture_pack_bind_rainfall(void) {
    rfTextureCacheSetOverride(acTexPackOverride);
}

#ifdef __cplusplus
}
#endif
