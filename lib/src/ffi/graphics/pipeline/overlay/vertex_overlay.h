#ifndef ___APRIORI2_GRAPHICS_PIPELINE_OVERLAY_VERTEX_H___
#define ___APRIORI2_GRAPHICS_PIPELINE_OVERLAY_VERTEX_H___

#include "ffi/graphics/gpu.h"
#include "ffi/graphics/pipeline/overlay/gpu_info.h"

struct VertexOVL {
    vk_location(OVL_VERTEX_INPUT_LOCATION_POS)
    float2 pos semantics(POSITION);

    vk_location(OVL_VERTEX_INPUT_LOCATION_COLOR)
    float4 color semantics(COLOR);

    vk_location(OVL_VERTEX_INPUT_LOCATION_TEXTURE)
    float2 tex semantics(TEXCOORD);
};

#endif // ___APRIORI2_GRAPHICS_PIPELINE_OVERLAY_VERTEX_H___