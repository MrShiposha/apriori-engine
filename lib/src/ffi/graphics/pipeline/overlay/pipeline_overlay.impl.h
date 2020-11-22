#ifndef ___APRIORI2_GRAPHICS_PIPELINE_OVERLAY_IMPL_H___
#define ___APRIORI2_GRAPHICS_PIPELINE_OVERLAY_IMPL_H___

#include "mod.h"

#define OVL_VERTEX_INPUT_BINDING 0

struct PipelineOVL {
    VkDevice device;
    VkShaderModule vertex_shader;
    VkShaderModule fragment_shader;
    VkSampler sampler;
    VkDescriptorSetLayout descr_set_layout;
    VkPipelineLayout layout;
    VkPipeline vk_handle;
};

#endif // ___APRIORI2_GRAPHICS_PIPELINE_OVERLAY_IMPL_H___