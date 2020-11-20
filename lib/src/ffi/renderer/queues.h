#ifndef ___APRIORI2_RENDERER_QUEUES_H___
#define ___APRIORI2_RENDERER_QUEUES_H___

#include <vulkan/vulkan.h>
#include <stdbool.h>

#include "ffi/export/result.h"

// Indices inside the queue family
#define RENDERER_QUEUE_GRAPHICS_IDX 0
#define RENDERER_QUEUE_PRESENT_IDX 1

struct RendererQueueFamilies {
    uint32_t graphics_idx;
    uint32_t present_idx;
};

struct RendererQueues {
    VkQueue graphics;
    VkQueue present;
};

Result new_renderer_queue_families(
    VkPhysicalDevice phy_device,
    VkSurfaceKHR surface
);

bool is_same_queue_families(struct RendererQueueFamilies *families);

void drop_renderer_queue_families(struct RendererQueueFamilies *families);

Result new_renderer_queues(
    VkDevice device,
    struct RendererQueueFamilies *families,
    uint32_t queue_cis_count
);

void drop_renderer_queues(struct RendererQueues *queues);

#endif // ___APRIORI2_RENDERER_QUEUES_H___