#ifndef ___APRIORI2_GRAPHICS_SWAPCHAIN_H___
#define ___APRIORI2_GRAPHICS_SWAPCHAIN_H___

#include <vulkan/vulkan.h>

#include "ffi/core/def.h"
#include "ffi/core/result.h"
#include "renderer/queues.h"

struct RendererQueues;

struct Swapchain {
    VkDevice device;
    VkSwapchainKHR vk_handle;
    VkImage *images;
    VkImageView *views;
    uint32_t image_count;
};

struct SwapchainCreateParams {
    VkPhysicalDevice phy_device;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_format;
    VkExtent2D image_extent;
    struct RendererQueueFamilies *families;
};

Result new_swapchain(struct SwapchainCreateParams *params);

void drop_swapchain(struct Swapchain *swapchain);

#endif // ___APRIORI2_GRAPHICS_SWAPCHAIN_H___