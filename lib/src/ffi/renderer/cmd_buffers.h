#ifndef ___APRIORI2_RENDERER_CMD_BUFFERS_H___
#define ___APRIORI2_RENDERER_CMD_BUFFERS_H___

#include <vulkan/vulkan.h>
#include "ffi/export/result.h"
#include "queues.h"
#include "cmd_pools.h"

struct RendererCmdBuffers {
    VkDevice device;
    struct RendererCmdPools *cmd_pools;
    VkCommandBuffer *graphics;
    VkCommandBuffer *present;
    uint32_t buffers_count;
};

Result new_renderer_cmd_buffers(
    VkDevice device,
    struct RendererQueueFamilies *queues,
    struct RendererCmdPools *cmd_pools,
    uint32_t buffers_count
);

void drop_renderer_cmd_buffers(struct RendererCmdBuffers *cmd_buffers);

#endif // ___APRIORI2_RENDERER_CMD_BUFFERS_H___
