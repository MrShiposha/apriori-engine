#ifndef ___APRIORI2_GRAPHICS_RENDERER_CMD_POOLS_H___
#define ___APRIORI2_GRAPHICS_RENDERER_CMD_POOLS_H___

#include <vulkan/vulkan.h>
#include "queues.h"

#include "ffi/export/result.h"

struct RendererCmdPools {
    VkDevice device;
    VkCommandPool graphics;
    VkCommandPool present;
};

Result new_renderer_cmd_pools(VkDevice device, struct RendererQueueFamilies *queues);

void drop_renderer_cmd_pools(struct RendererCmdPools *cmd_pools);

#endif // ___APRIORI2_GRAPHICS_RENDERER_CMD_POOLS_H___
