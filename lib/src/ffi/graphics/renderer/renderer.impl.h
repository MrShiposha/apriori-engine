#ifndef ___APRIORI2_GRAPHICS_RENDERER_IMPL_H___
#define ___APRIORI2_GRAPHICS_RENDERER_IMPL_H___

#include <vulkan/vulkan.h>
#include "ffi/core/vulkan_instance/mod.h"
#include "ffi/graphics/swapchain.h"

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

#endif // ___APRIORI2_GRAPHICS_RENDERER_IMPL_H___
