#include "util.h"
#include "log.h"
#include "swapchain.h"
#include "renderer/mod.h"

#define LOG_TARGET "Swapchain"

Result new_swapchain(struct SwapchainCreateParams *params) {
    ASSERT_NOT_NULL(params);
    ASSERT_NOT_NULL(params->phy_device);
    ASSERT_NOT_NULL(params->device);
    ASSERT_NOT_NULL(params->families);

    Result result = { 0 };
    uint32_t present_modes_count = 0;
    VkPresentModeKHR *present_modes = NULL;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t avg_image_count = 0;
    uint32_t queues[] = {
        params->families->graphics_idx,
        params->families->present_idx,
    };
    VkImageViewCreateInfo image_view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D
    };
    VkComponentMapping iv_components = {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    };
    VkImageSubresourceRange iv_subresource_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseArrayLayer = 0,
        .baseMipLevel = 0,
        .layerCount = 1,
        .levelCount = 1
    };
    VkSwapchainCreateInfoKHR swapchain_ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR
    };

    trace(LOG_TARGET, "creating new swapchain...");

    struct Swapchain *swapchain = calloc(1, sizeof(struct Swapchain));
    result.object = swapchain;
    EXPECT_MEM_ALLOC(result);

    trace(LOG_TARGET, "\tgetting surface capabilities...");
    VkSurfaceCapabilitiesKHR surface_caps = { 0 };
    result.error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        params->phy_device,
        params->surface,
        &surface_caps
    );
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, "\tselecting present mode...");
    result.error = vkGetPhysicalDeviceSurfacePresentModesKHR(
        params->phy_device,
        params->surface,
        &present_modes_count,
        NULL
    );
    EXPECT_SUCCESS(result);

    result.object = calloc(present_modes_count, sizeof(VkPresentModeKHR));
    EXPECT_MEM_ALLOC(result);
    present_modes = result.object;

    result.error = vkGetPhysicalDeviceSurfacePresentModesKHR(
        params->phy_device,
        params->surface,
        &present_modes_count,
        present_modes
    );
    EXPECT_SUCCESS(result);

    for (uint32_t i = 0; i < present_modes_count; ++i) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            trace(LOG_TARGET, "\tfound MAILBOX present mode");
            present_mode = present_modes[i];
            break;
        }
    }

    trace(LOG_TARGET, "\tpresent mode selected");

    avg_image_count = (
        surface_caps.minImageCount + surface_caps.maxImageCount
    ) / 2;

    swapchain_ci.surface = params->surface;
    swapchain_ci.minImageCount = avg_image_count;
    swapchain_ci.imageFormat = params->surface_format.format;
    swapchain_ci.imageColorSpace = params->surface_format.colorSpace;
    swapchain_ci.imageExtent = params->image_extent;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_ci.preTransform = surface_caps.currentTransform;
    swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.oldSwapchain = VK_NULL_HANDLE;

    if (queues[0] == queues[1]) {
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_ci.queueFamilyIndexCount = STATIC_ARRAY_SIZE(queues);
        swapchain_ci.pQueueFamilyIndices = queues;
    }

    result.error = vkCreateSwapchainKHR(
        params->device,
        &swapchain_ci,
        NULL,
        &swapchain->vk_handle
    );
    result.object = swapchain;
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, "\tgetting swapchain images...");
    result.error = vkGetSwapchainImagesKHR(
        params->device,
        swapchain->vk_handle,
        &swapchain->image_count,
        NULL
    );
    EXPECT_SUCCESS(result);

    swapchain->device = params->device;

    swapchain->images = calloc(swapchain->image_count, sizeof(VkImage));
    result.object = swapchain->images;
    EXPECT_MEM_ALLOC(result);

    result.error = vkGetSwapchainImagesKHR(
        params->device,
        swapchain->vk_handle,
        &swapchain->image_count,
        swapchain->images
    );
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, "\tswapchain images received successufully");

    trace(LOG_TARGET, "\tcreating new swapchain image views...");

    swapchain->views = calloc(swapchain->image_count, sizeof(VkImageView));
    result.object = swapchain->views;
    EXPECT_MEM_ALLOC(result);

    image_view_ci.format = params->surface_format.format;
    image_view_ci.components = iv_components;
    image_view_ci.subresourceRange = iv_subresource_range;

    for (uint32_t i = 0; i < swapchain->image_count; ++i) {
        image_view_ci.image = AS(swapchain->images, VkImage*)[i];

        result.error = vkCreateImageView(
            params->device,
            &image_view_ci,
            NULL,
            &AS(swapchain->views, VkImageView*)[i]
        );
        EXPECT_SUCCESS(result);
    }

    trace(LOG_TARGET, "\tnew swapchain image views created successfully");

    result.object = swapchain;
    trace(LOG_TARGET, "new swapchain created successfully");

    free(present_modes);
    return result;

failure:
    free(present_modes);
    drop_swapchain(swapchain);

    return result;
}

void drop_swapchain(struct Swapchain *swapchain) {
    if (swapchain == NULL)
        return;

    for (uint32_t i = 0; i < swapchain->image_count; ++i) {
        vkDestroyImageView(
            swapchain->device,
            AS(swapchain->views, VkImageView*)[i],
            NULL
        );
    }

    free(swapchain->views);
    free(swapchain->images);

    vkDestroySwapchainKHR(swapchain->device, swapchain->vk_handle, NULL);
    free(swapchain);
}
