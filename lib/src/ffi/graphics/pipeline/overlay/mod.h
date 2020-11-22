#ifndef ___APRIORI2_GRAPHICS_PIPELINE_OVERLAY_H___
#define ___APRIORI2_GRAPHICS_PIPELINE_OVERLAY_H___

#include <vulkan/vulkan.h>

#include "ffi/core/result.h"
#include "ffi/util/mod.h"
#include "ffi/math/mod.h"

#define OVL_COMBINED_IMAGE_SAMPLER_DESCR_IDX 0

#define OVL_PUSH_CONSTANT_POSITION_SIZE sizeof(Coord)
#define OVL_PUSH_CONSTANT_IMAGE_EXTENT_SIZE sizeof(VkExtent2D)

#define PUSH_CONSTANTS_SIZE_COUNT_UNITS 4

#define PUSH_CONSTANTS_RAW_SIZE ( \
    OVL_PUSH_CONSTANT_POSITION_SIZE \
    + OVL_PUSH_CONSTANT_IMAGE_EXTENT_SIZE \
)

#define OVL_PUSH_CONSTANTS_SIZE AS_BINARY_COUNT_UNITS( \
    PUSH_CONSTANTS_RAW_SIZE, \
    PUSH_CONSTANTS_SIZE_COUNT_UNITS, \
    uint32_t \
)

struct PipelineOVL;

Result new_pipeline_ovl(
    VkDevice device,
    VkRenderPass render_pass,
    const VkExtent2D *render_target_extent,
    uint32_t render_target_count,
    float *anisotropy
);

void drop_pipeline_ovl(struct PipelineOVL *pipeline);

Result get_ovl_descriptor_pool_sizes();

#endif // ___APRIORI2_GRAPHICS_PIPELINE_OVERLAY_H___