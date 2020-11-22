#ifndef ___APRIORI2_GRAPHICS_RENDERER_IMPL_H___
#define ___APRIORI2_GRAPHICS_RENDERER_IMPL_H___

#include <vulkan/vulkan.h>
#include "ffi/core/vulkan_instance/mod.h"
#include "ffi/graphics/swapchain.h"
#include "ffi/graphics/pipeline/overlay/mod.h"

#include "queues.h"
#include "cmd_pools.h"
#include "cmd_buffers.h"

struct RendererPools {
    struct RendererCmdPools *cmd;
    VkDescriptorPool descr;
};

struct RendererBuffers {
    struct RendererCmdBuffers *cmd;
};

struct RendererPipelines {
    struct PipelineOVL *overlay;
};

struct RendererFFI {
    VulkanInstance vk_instance;
    VkDevice gpu;
    VkSurfaceKHR surface;
    struct Swapchain *swapchain;
    struct RendererQueues *queues;
    struct RendererPools pools;
    struct RendererBuffers buffers;
    VkRenderPass render_pass;

    struct RendererPipelines pipelines;
};

#endif // ___APRIORI2_GRAPHICS_RENDERER_IMPL_H___
