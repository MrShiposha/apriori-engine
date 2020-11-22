#pragma shader_stage fragment

#include "ffi/graphics/gpu.h"
#include "ffi/graphics/pipeline/overlay/gpu_info.h"

float4 main(
    vk_location(OVL_FRAGMENT_INPUT_LOCATION_COLOR)
    float4 color semantics(COLOR)
) semantics(SV_TARGET)
{
    return color;
}
