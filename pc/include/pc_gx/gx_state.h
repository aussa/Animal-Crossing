/**
 * pc/include/pc_gx/gx_state.h
 *
 * GX State Management System for Twilight Princess PC Port
 *
 * This header defines comprehensive state structures that track all GX function
 * calls, making the state available for shader generation and rendering.
 *
 * The GX API is the GameCube's graphics API. Rather than directly executing
 * hardware commands, our PC implementation captures all state and translates
 * it for Rainfall's GPU pipeline.
 *
 * Reference: GX Spec defines TEV with 16 stages, 8 texcoords, 8 texture maps
 */

#ifndef PC_GX_STATE_H
#define PC_GX_STATE_H

#include "dolphin/types.h"
#include "dolphin/gx/GXEnum.h"
#include "dolphin/gx/GXStruct.h"  // For GXColor, GXColorS10

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constants
// =============================================================================

#define GX_MAX_TEXMTX      10
#define GX_MAX_PTTEXMTX    20
#define GX_MAX_PNMTX       20
#define GX_MAX_LIGHTS       8

// =============================================================================
// Dirty Flags - Used to track which parts of state need updating
// =============================================================================

typedef enum GXDirtyFlags {
    GX_DIRTY_NONE       = 0,
    GX_DIRTY_TEV        = 1 << 0,   // TEV stage configuration changed
    GX_DIRTY_TEXTURE    = 1 << 1,   // Texture binding or parameters changed
    GX_DIRTY_BLEND      = 1 << 2,   // Blend mode changed
    GX_DIRTY_VERTEX     = 1 << 3,   // Vertex format changed
    GX_DIRTY_MATRIX     = 1 << 4,   // Transform matrices changed
    GX_DIRTY_VIEWPORT   = 1 << 5,   // Viewport/scissor changed
    GX_DIRTY_LIGHTING   = 1 << 6,   // Lighting state changed
    GX_DIRTY_FOG        = 1 << 7,   // Fog state changed
    GX_DIRTY_ALPHA      = 1 << 8,   // Alpha compare changed
    GX_DIRTY_ZMODE      = 1 << 9,   // Z mode changed
    GX_DIRTY_CULL       = 1 << 10,  // Cull mode changed
    GX_DIRTY_TEXGEN     = 1 << 11,  // Texture coordinate generation changed
    GX_DIRTY_INDIRECT   = 1 << 12,  // Indirect texturing changed
    GX_DIRTY_ZTEX       = 1 << 13,  // Z-texture / Z compare location changed
    GX_DIRTY_ALL        = 0xFFFFFFFF
} GXDirtyFlags;

// =============================================================================
// Color Structures
// =============================================================================

// Use GXColor from GXStruct.h for standard 8-bit per channel color
// Use GXColorS10 from GXStruct.h for signed 10-bit per channel color (TEV)

// Alias for internal use (same as GXColor)
typedef GXColor GXColorU8;

// Float color for shader uniforms (not in GXStruct.h)
#ifdef TARGET_PC
#include <dolphin/gx/GXExtra.h>
#else
typedef struct GXColorF32 {
    f32 r, g, b, a;
} GXColorF32;
#endif

// =============================================================================
// TEV (Texture Environment) Stage Configuration
// =============================================================================

// Color combiner inputs (a, b, c, d)
typedef struct GXTevColorPass {
    GXTevColorArg a;
    GXTevColorArg b;
    GXTevColorArg c;
    GXTevColorArg d;
} GXTevColorPass;

// Alpha combiner inputs (a, b, c, d)
typedef struct GXTevAlphaPass {
    GXTevAlphaArg a;
    GXTevAlphaArg b;
    GXTevAlphaArg c;
    GXTevAlphaArg d;
} GXTevAlphaPass;

// TEV operation configuration
typedef struct GXTevOpConfig {
    GXTevOp op;
    GXTevBias bias;
    GXTevScale scale;
    GXTevRegID outReg;
    GXBool clamp;
} GXTevOpConfig;

// Complete TEV stage configuration
typedef struct GXTevStage {
    // Color combiner
    GXTevColorPass colorIn;
    GXTevOpConfig colorOp;

    // Alpha combiner
    GXTevAlphaPass alphaIn;
    GXTevOpConfig alphaOp;

    // Konstant color/alpha selection
    GXTevKColorSel kcSel;
    GXTevKAlphaSel kaSel;

    // Texture and color channel inputs
    GXTexCoordID texCoordId;
    GXTexMapID texMapId;
    GXChannelID channelId;

    // Swap mode selection
    GXTevSwapSel rasSwap;
    GXTevSwapSel texSwap;

    // Indirect texturing
    GXIndTexStageID indTexStage;
    GXIndTexFormat indTexFormat;
    GXIndTexBiasSel indTexBiasSel;
    GXIndTexAlphaSel indTexAlphaSel;
    GXIndTexMtxID indTexMtxId;
    GXIndTexWrap indTexWrapS;
    GXIndTexWrap indTexWrapT;
    GXBool indTexUseOrigLOD;
    GXBool indTexAddPrev;
} GXTevStage;

// TEV swap table entry
typedef struct GXTevSwap {
    GXTevColorChan red;
    GXTevColorChan green;
    GXTevColorChan blue;
    GXTevColorChan alpha;
} GXTevSwap;

// =============================================================================
// Texture Coordinate Generation
// =============================================================================

typedef struct GXTexCoordGen {
    GXTexGenType type;
    GXTexGenSrc src;
    GXTexMtx mtx;
    GXPTTexMtx postMtx;
    GXBool normalize;
} GXTexCoordGen;

// =============================================================================
// Indirect Texture Stage
// =============================================================================

typedef struct GXIndStage {
    GXTexCoordID texCoordId;
    GXTexMapID texMapId;
    GXIndTexScale scaleS;
    GXIndTexScale scaleT;
} GXIndStage;

// Indirect texture matrix
typedef struct GXIndTexMtxInfo {
    f32 mtx[2][3];
    s8 scaleExp;
} GXIndTexMtxInfo;

// =============================================================================
// Channel (Lighting) Configuration
// =============================================================================

typedef struct GXChannelConfig {
    GXColorSrc matSrc;
    GXColorSrc ambSrc;
    GXDiffuseFn diffFn;
    GXAttnFn attnFn;
    GXBool lightingEnabled;
    u32 lightMask;      // Bitmask of GXLightID values
} GXChannelConfig;

// Light object state
typedef struct GXLightState {
    GXColorF32 color;
    f32 posX, posY, posZ;
    f32 dirX, dirY, dirZ;
    f32 a0, a1, a2;     // Angle attenuation
    f32 k0, k1, k2;     // Distance attenuation
} GXLightState;

// =============================================================================
// Vertex Attribute Format
// =============================================================================

typedef struct GXVtxAttrFmt {
    GXCompCnt cnt;
    GXCompType type;
    u8 frac;
} GXVtxAttrFmt;

// Per-vertex-format attribute configuration
typedef struct GXVtxFmtConfig {
    GXVtxAttrFmt attrs[GX_VA_MAX_ATTR];
} GXVtxFmtConfig;

// Vertex array pointer
typedef struct GXVtxArray {
    const void* data;
    u32 size;
    u8 stride;
} GXVtxArray;

// =============================================================================
// Fog State
// =============================================================================

typedef struct GXFogState {
    GXFogType type;
    f32 startZ;
    f32 endZ;
    f32 nearZ;
    f32 farZ;
    GXColorF32 color;
} GXFogState;

// =============================================================================
// Alpha Compare State
// =============================================================================

typedef struct GXAlphaCompareState {
    GXCompare comp0;
    u8 ref0;
    GXAlphaOp op;
    GXCompare comp1;
    u8 ref1;
} GXAlphaCompareState;

// =============================================================================
// Blend Mode State
// =============================================================================

typedef struct GXBlendState {
    GXBlendMode mode;
    GXBlendFactor srcFactor;
    GXBlendFactor dstFactor;
    GXLogicOp logicOp;
} GXBlendState;

// =============================================================================
// Z Mode State
// =============================================================================

typedef struct GXZModeState {
    GXBool enable;
    GXCompare func;
    GXBool updateEnable;
} GXZModeState;

typedef struct GXZTextureState {
    GXZTexOp op;
    GXTexFmt fmt;
    u32 bias;
} GXZTextureState;

// =============================================================================
// Texture Object Tracking
// =============================================================================

typedef struct GXTexObjState {
    GXBool valid;
    const void* data;
    u16 width;
    u16 height;
    GXTexFmt format;
    GXTexWrapMode wrapS;
    GXTexWrapMode wrapT;
    GXTexFilter minFilter;
    GXTexFilter magFilter;
    f32 minLOD;
    f32 maxLOD;
    f32 lodBias;
    u32 tlutName;
    u8 maxAniso;
    u8 useIndexedSampling;
    void* gpuTexture;
} GXTexObjState;

// =============================================================================
// Matrix Types
// =============================================================================

typedef f32 GXMtx34[3][4];      // 3x4 matrix (position/normal)
typedef f32 GXMtx44[4][4];      // 4x4 matrix (projection)

// Position/Normal matrix pair
typedef struct GXPnMtx {
    GXMtx34 pos;
    GXMtx34 nrm;
} GXPnMtx;

// =============================================================================
// Main GX State Structure
// =============================================================================

typedef struct GXState {
    // =========================================================================
    // Vertex Format State
    // =========================================================================

    // Vertex descriptor (which attributes are present and their type)
    GXAttrType vtxDesc[GX_VA_MAX_ATTR];

    // Vertex attribute formats (per vertex format slot)
    GXVtxFmtConfig vtxFmts[GX_MAX_VTXFMT];

    // Vertex array pointers
    GXVtxArray vtxArrays[GX_VA_MAX_ATTR];

    // =========================================================================
    // TEV State
    // =========================================================================

    // TEV stages (up to 16)
    GXTevStage tevStages[GX_MAX_TEVSTAGE];
    u8 numTevStages;

    // TEV registers (TEVPREV, TEVREG0-2)
    GXColorS10 tevRegs[GX_MAX_TEVREG];

    // Konstant colors (KCOLOR0-3)
    GXColorU8 tevKColors[GX_MAX_KCOLOR];

    // TEV swap tables (4 tables)
    GXTevSwap tevSwapTable[GX_MAX_TEVSWAP];

    // =========================================================================
    // Texture State
    // =========================================================================

    // Texture objects bound to each texture map slot
    GXTexObjState textures[GX_MAX_TEXMAP];

    // Texture coordinate generation
    GXTexCoordGen texCoordGens[GX_MAX_TEXCOORD];
    u8 numTexGens;

    // Texture matrices (10 slots: GX_TEXMTX0-9)
    GXMtx34 texMtx[GX_MAX_TEXMTX];
    GXBool texMtxValid[GX_MAX_TEXMTX];

    // Post-transform texture matrices (20 slots)
    GXMtx34 ptTexMtx[GX_MAX_PTTEXMTX];
    GXBool ptTexMtxValid[GX_MAX_PTTEXMTX];

    // =========================================================================
    // Indirect Texturing State
    // =========================================================================

    GXIndStage indStages[GX_MAX_INDTEXSTAGE];
    GXIndTexMtxInfo indTexMtx[3];  // ITM_0, ITM_1, ITM_2
    u8 numIndStages;

    // =========================================================================
    // Transform State
    // =========================================================================

    // Position/Normal matrices (10 slots)
    GXPnMtx pnMtx[GX_MAX_PNMTX];
    u32 currentPnMtx;

    // Projection matrix
    GXMtx44 projection;
    GXProjectionType projType;

    // =========================================================================
    // Viewport and Scissor
    // =========================================================================

    f32 vpLeft, vpTop, vpWidth, vpHeight;
    f32 vpNearZ, vpFarZ;

    u32 scissorLeft, scissorTop, scissorWidth, scissorHeight;

    // =========================================================================
    // Pixel Processing State
    // =========================================================================

    // Blend mode
    GXBlendState blend;

    // Alpha compare
    GXAlphaCompareState alphaCompare;

    // Z mode
    GXZModeState zMode;

    // Z-texture and compare location
    GXZTextureState zTexture;
    GXBool zCompLoc;

    // Cull mode
    GXCullMode cullMode;

    // Color/Alpha write masks
    GXBool colorUpdate;
    GXBool alphaUpdate;

    // Destination alpha
    GXBool dstAlphaEnabled;
    u8 dstAlpha;

    // =========================================================================
    // Lighting State
    // =========================================================================

    // Channel controls (COLOR0, COLOR1, ALPHA0, ALPHA1 = 4 channels)
    GXChannelConfig channelConfig[4];
    u8 numChans;

    // Material colors (per channel)
    GXColorU8 chanMatColor[4];

    // Ambient colors (per channel)
    GXColorU8 chanAmbColor[4];

    // Light objects
    GXLightState lights[GX_MAX_LIGHTS];

    // =========================================================================
    // Fog State
    // =========================================================================

    GXFogState fog;

    // =========================================================================
    // Clear State
    // =========================================================================

    GXColorU8 clearColor;
    u32 clearZ;

    // =========================================================================
    // Dirty Tracking
    // =========================================================================

    u32 dirtyFlags;

} GXState;

// =============================================================================
// Global State Instance
// =============================================================================

#ifdef __cplusplus
}
#endif

#endif // PC_GX_STATE_H
