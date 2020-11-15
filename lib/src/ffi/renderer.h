#ifndef ___APRIORI2_RENDERER_H___
#define ___APRIORI2_RENDERER_H___

#include <vulkan/vulkan.h>
#include "ffi/export/vulkan_instance.h"

struct RendererQueues {
    uint32_t graphics_idx;
    uint32_t present_idx;

    VkQueue graphics;
    VkQueue present;
};

struct RendererFFI {
    VulkanInstance vk_instance;
    VkDevice gpu;
    struct RendererQueues queues;
};

#endif // ___APRIORI2_RENDERER_H___
