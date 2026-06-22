#pragma once

#ifndef PC_GX_RAINFALL_COMPAT_H
#define PC_GX_RAINFALL_COMPAT_H

/*
 * Rainfall compatibility header for the TP PC port.
 * TP provides GX state types; Rainfall provides rendering.
 */

#include "pc_gx/gx_state.h"
#include "pc_gx/gx_texture.h"
#include <rainfall/render/gx/gx_state.h>
#include <rainfall/render/gpu/gpu.h>

typedef RFGPUVertUniforms   GPUVertUniforms;
typedef RFGPUFragUniforms   GPUFragUniforms;
typedef RFGPURenderState    GPURenderState;
typedef RFGPUDrawCommand    GPUDrawCommand;
typedef RFGPUFrameData      GPUFrameData;
typedef RFGPUTexBinding     GPUTexBinding;
typedef RFGPUViewport       GPUViewport;
typedef RFGPUScissor        GPUScissor;
typedef RFGPUEFBCopyCmd     GPUEFBCopyCmd;

#define GPU_FIXED_VERTEX_STRIDE RF_GPU_FIXED_VERTEX_STRIDE

#ifdef __cplusplus
extern "C" {
#endif

extern RFGXState g_gxSubmitState;

#ifdef __cplusplus
}
#endif

#endif /* PC_GX_RAINFALL_COMPAT_H */
