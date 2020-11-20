#ifndef ___APRIORI2_RENDERER_H___
#define ___APRIORI2_RENDERER_H___

#include <vulkan/vulkan.h>
#include "ffi/export/vulkan_instance.h"
#include "ffi/export/dyn_array.h"
#include "ffi/swapchain.h"

#include "queues.h"
#include "cmd_pools.h"
#include "cmd_buffers.h"

struct RendererPools {
    struct RendererCmdPools *cmd;
};

struct RendererBuffers {
    struct RendererCmdBuffers *cmd;
};

struct RendererFFI {
    VulkanInstance vk_instance;
    VkDevice gpu;
    VkSurfaceKHR surface;
    struct Swapchain *swapchain;
    struct RendererQueues *queues;
    struct RendererPools pools;
    struct RendererBuffers buffers;
};

#endif // ___APRIORI2_RENDERER_H___
