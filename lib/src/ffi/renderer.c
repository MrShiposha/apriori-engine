#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>

#include "ffi/export/renderer.h"
#include "renderer.h"
#include "vulkan_instance.h"
#include "ffi/export/def.h"
#include "ffi/export/error.h"
#include "ffi/export/vulkan_instance.h"
#include "ffi/result_fns.h"
#include "ffi/os/surface.h"
#include "ffi/log.h"
#include "ffi/util.h"
#include "ffi/export/dyn_array.h"

#define LOG_TARGET "FFI/Renderer"
#define CI_GRAPHICS_IDX 0
#define CI_PRESENT_IDX 1

uint32_t rate_phy_device_suitability(VkPhysicalDeviceProperties *dev_props) {
    ASSERT_NOT_NULL(dev_props);

    uint32_t score = 0;

    switch(dev_props->deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        score += 1000;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        score += 100;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        score += 10;
        break;
    }

    trace(
        LOG_TARGET,
        "\t\"%s\" physical device suitability score: %d",
        dev_props->deviceName, score
    );

    return score;
}

VkPhysicalDevice select_phy_device(VulkanInstance instance) {
    uint32_t score = 0;
    uint32_t current_score = 0;
    VkPhysicalDeviceProperties dev_props = { 0 };
    VkPhysicalDevice winner_device = VK_NULL_HANDLE;
    char winner_device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = { 0 };

    trace(
        LOG_TARGET,
        "selecting the most suitable physical device..."
    );

    for (uint32_t i = 0; i < instance->phy_device_count; ++i) {
        vkGetPhysicalDeviceProperties(instance->phy_devices[i], &dev_props);

        current_score = rate_phy_device_suitability(
            &dev_props
        );

        if (current_score > score) {
            score = current_score;
            winner_device = instance->phy_devices[i];
            errno_t copy_result = strcpy_s(
                winner_device_name,
                VK_MAX_PHYSICAL_DEVICE_NAME_SIZE,
                dev_props.deviceName
            );

            assert(copy_result == 0 && "unable to copy physical device name");
        }
    }

    assert(
        winner_device != VK_NULL_HANDLE
        && "Renderer: physical device must be selected"
    );

    trace(
        LOG_TARGET,
        "the most suitable physical device: \"%s\" (score: %d)",
        winner_device_name, score
    );

    return winner_device;
}

Result phy_device_surface_formats(VkPhysicalDevice phy_device, VkSurfaceKHR surface) {
    ASSERT_NOT_NULL(phy_device);
    ASSERT_NOT_NULL(surface);

    Result result = { 0 };
    uint32_t surface_formats_count = 0;
    DynArray surface_formats = NULL;

    result.error = vkGetPhysicalDeviceSurfaceFormatsKHR(
        phy_device,
        surface,
        &surface_formats_count,
        NULL
    );
    EXPECT_SUCCESS(result);

    result = NEW_DYN_ARRAY(surface_formats_count, VkSurfaceFormatKHR);
    RESULT_UNWRAP(surface_formats, result);

    result.error = vkGetPhysicalDeviceSurfaceFormatsKHR(
        phy_device,
        surface,
        &surface_formats->count,
        AS(surface_formats->data, VkSurfaceFormatKHR *)
    );
    EXPECT_SUCCESS(result);

    result.object = surface_formats;
    return result;

failure:
    free(surface_formats);
    return result;
}

VkSurfaceFormatKHR select_surface_format(DynArray surface_formats) {
    ASSERT_NOT_NULL(surface_formats);
    assert(surface_formats->count > 0 && "surface format count must be greater than 0");

    VkSurfaceFormatKHR *formats = AS(surface_formats->data, VkSurfaceFormatKHR *);
    VkSurfaceFormatKHR format = formats[0];

    for (uint32_t i = 0; i < surface_formats->count; ++i) {
        if (
            formats[i].format == VK_FORMAT_B8G8R8A8_SRGB
            && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            trace(LOG_TARGET, "found SRGB B8G8R8A8 format");
            format = formats[i];
            break;
        }
    }

    return format;
}

Result new_swapchain(VkPhysicalDevice phy_device, Renderer renderer) {
    ASSERT_NOT_NULL(phy_device);
    ASSERT_NOT_NULL(renderer);

    Result result = { 0 };
    VkSwapchainCreateInfoKHR swapchain_ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR
    };
    VkSwapchainKHR swapchain;
    uint32_t present_modes_count = 0;
    VkPresentModeKHR *present_modes = NULL;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    trace(LOG_TARGET, "creating new swapchain...");

    swapchain_ci.surface = renderer->surface;
    swapchain_ci.minImageCount = renderer->images_count;

    result.error = vkGetPhysicalDeviceSurfacePresentModesKHR(
        phy_device,
        renderer->surface,
        &present_modes_count,
        NULL
    );
    EXPECT_SUCCESS(result);

    present_modes = calloc(present_modes_count, sizeof(VkPresentModeKHR));
    if (present_modes == NULL) {
        result.error = OUT_OF_MEMORY;
        goto failure;
    }

    result.error = vkGetPhysicalDeviceSurfacePresentModesKHR(
        phy_device,
        renderer->surface,
        &present_modes_count,
        present_modes
    );
    EXPECT_SUCCESS(result);

    for (uint32_t i = 0; i < present_modes_count; ++i) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            present_mode = present_modes[i];
    }

    swapchain_ci.imageFormat = renderer->surface_format.format;
    swapchain_ci.imageColorSpace = renderer->surface_format.colorSpace;
    swapchain_ci.imageExtent = renderer->image_extent;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    swapchain_ci.presentMode = present_mode;

    uint32_t queue_families[] = {
        renderer->queues.graphics_family_idx,
        renderer->queues.present_family_idx
    };

    if (renderer->queues.graphics_family_idx == renderer->queues.present_family_idx) {
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_ci.queueFamilyIndexCount = STATIC_ARRAY_SIZE(queue_families);
        swapchain_ci.pQueueFamilyIndices = queue_families;
    }

    swapchain_ci.preTransform = renderer->surface_caps.currentTransform;
    swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.oldSwapchain = VK_NULL_HANDLE;

    result.error = vkCreateSwapchainKHR(
        renderer->gpu,
        &swapchain_ci,
        NULL,
        &swapchain
    );
    result.object = swapchain;
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, "new swapchain created successfully");

failure:
    free(present_modes);

    return result;
}

Apriori2Error find_renderer_queue_family_indices(
    VkPhysicalDevice phy_device,
    struct RendererQueues *queues,
    VkSurfaceKHR surface
) {
    ASSERT_NOT_NULL(queues);

    Apriori2Error error = SUCCESS;

    trace(
        LOG_TARGET,
        "searching for renderer queue family indices..."
    );

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        phy_device,
        &queue_family_count,
        NULL
    );

    trace(
        LOG_TARGET,
        "queue family count: %d",
        queue_family_count
    );

    VkQueueFamilyProperties *family_props = calloc(
        queue_family_count, sizeof(VkQueueFamilyProperties)
    );
    if (family_props == NULL)
        return OUT_OF_MEMORY;

    vkGetPhysicalDeviceQueueFamilyProperties(
        phy_device,
        &queue_family_count,
        family_props
    );

    bool is_graphics_queue_found = false;
    bool is_present_queue_found = false;

    VkBool32 is_present_support = false;

    VkQueueFamilyProperties *current = NULL;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        current = family_props + i;

        error = vkGetPhysicalDeviceSurfaceSupportKHR(
            phy_device,
            i,
            surface,
            &is_present_support
        );

        if (error != VK_SUCCESS)
            return error;

        if (
            (current->queueFlags & VK_QUEUE_GRAPHICS_BIT)
            && is_present_support
        ) {
            queues->graphics_family_idx = i;
            queues->present_family_idx = i;

            is_graphics_queue_found = true;
            is_present_queue_found = true;
            break;
        }

        if (current->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queues->graphics_family_idx = i;
            is_graphics_queue_found = true;
        }

        if (is_present_support) {
            queues->present_family_idx = i;
            is_present_queue_found = true;
        }
    }

    free(family_props);

    if (!is_graphics_queue_found && !is_present_queue_found)
        error = RENDERER_QUEUE_FAMILIES_NOT_FOUND;
    else if (!is_graphics_queue_found)
        error = GRAPHICS_QUEUE_FAMILY_NOT_FOUND;
    else if (!is_present_queue_found)
        error = PRESENT_QUEUE_FAMILY_NOT_FOUND;
    else {
        error = SUCCESS;

        trace(
            LOG_TARGET,
            "renderer queues:"
        );

        trace(
            LOG_TARGET,
            "\tgraphics queue family idx: %d",
            queues->graphics_family_idx
        );

        trace(
            LOG_TARGET,
            "\tpresent queue family idx: %d",
            queues->present_family_idx
        );
    }

    return error;
}

void fill_renderer_queues_create_info(
    struct RendererQueues *queues,
    uint32_t *queues_cis_count,
    VkDeviceQueueCreateInfo *queues_cis
) {
    ASSERT_NOT_NULL(queues);
    ASSERT_NOT_NULL(queues_cis_count);
    ASSERT_NOT_NULL(queues_cis);

    static float priorities[2] = { 0 };

    trace(LOG_TARGET, "filling renderer queues create info...");

    priorities[CI_GRAPHICS_IDX] = 1.0f;
    priorities[CI_PRESENT_IDX] = 0.75f;

    if (queues->graphics_family_idx == queues->present_family_idx) {
        uint32_t family_idx = queues->graphics_family_idx;

        *queues_cis_count = 1;
        queues->graphics_local_idx = 0;
        queues->present_local_idx = 0;

        queues_cis->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queues_cis->queueCount = 2;
        queues_cis->queueFamilyIndex = family_idx;
        queues_cis->pQueuePriorities = priorities;
    } else {
        *queues_cis_count = 2;
        queues->graphics_local_idx = CI_GRAPHICS_IDX;
        queues->present_local_idx = CI_PRESENT_IDX;

        queues_cis[CI_GRAPHICS_IDX].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queues_cis[CI_GRAPHICS_IDX].queueCount = 1;
        queues_cis[CI_GRAPHICS_IDX].queueFamilyIndex = queues->graphics_family_idx;
        queues_cis[CI_GRAPHICS_IDX].pQueuePriorities = &priorities[CI_GRAPHICS_IDX];

        queues_cis[CI_PRESENT_IDX].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queues_cis[CI_PRESENT_IDX].queueCount = 1;
        queues_cis[CI_PRESENT_IDX].queueFamilyIndex = queues->present_family_idx;
        queues_cis[CI_PRESENT_IDX].pQueuePriorities = &priorities[CI_PRESENT_IDX];
    }
}

Result check_all_device_layers_available(VkPhysicalDevice device, const char **layers, uint32_t num_layers) {
    Apriori2Error err = SUCCESS;
    VkLayerProperties *layer_props = NULL;
    uint32_t property_count = 0;

    trace(LOG_TARGET, "checking requested validation layers");

    err = vkEnumerateDeviceLayerProperties(device, &property_count, NULL);
    if (err != VK_SUCCESS) {
        goto exit;
    }

    trace(LOG_TARGET, "available validation layers count: %d", property_count);

    layer_props = malloc(property_count * sizeof(VkLayerProperties));
    if (layer_props == NULL) {
        err = OUT_OF_MEMORY;
        goto exit;
    }

    err = vkEnumerateDeviceLayerProperties(device, &property_count, layer_props);
    if (err != VK_SUCCESS)
        goto exit;

    for (uint32_t i = 0, j = 0; i < num_layers; ++i) {
        for (j = 0; j < property_count; ++j) {
            if (!strcmp(layers[i], layer_props[j].layerName))
                break;
        }

        if (j == property_count) {
            // Some layer was not found
            err = LAYERS_NOT_FOUND;
            error(LOG_TARGET, "layer \"%s\" is not found", layers[i]);
        } else {
            trace(LOG_TARGET, "\tvalidation layer \"%s\": OK", layers[i]);
        }
    }

exit:
    free(layer_props);
    return apriori2_error(err);
}

Result check_all_device_extensions_available(
    VkPhysicalDevice phy_device,
    const char **extensions,
    uint32_t num_extensions
) {
    Apriori2Error err = SUCCESS;
    VkExtensionProperties *extension_props = NULL;
    uint32_t property_count = 0;

    trace(LOG_TARGET, "checking requested extensions");

    err = vkEnumerateDeviceExtensionProperties(phy_device, NULL, &property_count, NULL);
    if (err != VK_SUCCESS) {
        goto exit;
    }

    trace(LOG_TARGET, "available extension count: %d", property_count);

    extension_props = malloc(property_count * sizeof(VkLayerProperties));
    if (extension_props == NULL) {
        err = OUT_OF_MEMORY;
        goto exit;
    }

    err = vkEnumerateDeviceExtensionProperties(phy_device, NULL, &property_count, extension_props);
    if (err != VK_SUCCESS)
        goto exit;

    for (uint32_t i = 0, j = 0; i < num_extensions; ++i) {
        for (j = 0; j < property_count; ++j) {
            if (!strcmp(extensions[i], extension_props[j].extensionName))
                break;
        }

        if (j == property_count) {
            // Some extensions was not found
            err = EXTENSIONS_NOT_FOUND;
            error(LOG_TARGET, "extension \"%s\" is not found", extensions[i]);
        } else {
            trace(LOG_TARGET, "\textension \"%s\": OK", extensions[i]);
        }
    }

exit:
    free(extension_props);
    return apriori2_error(err);
}

Result new_gpu(VkPhysicalDevice phy_device, struct RendererQueues *queues) {
    ASSERT_NOT_NULL(phy_device);
    ASSERT_NOT_NULL(queues);

    Result result = { 0 };
    uint32_t queues_cis_count = 0;
    VkDeviceQueueCreateInfo queues_cis[2] = { 0 };
    VkDeviceCreateInfo device_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
    };
    VkDevice gpu = VK_NULL_HANDLE;

    trace(LOG_TARGET, "creating new GPU object...");

#   ifdef ___debug___
    const char *layer_names[] = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    const uint32_t layer_names_count = STATIC_ARRAY_SIZE(layer_names);
#   else
    const char *layer_names = NULL;
    const uint32_t layer_names_count = 0;
#   endif // ___debug___

    const char *extension_names[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    result = check_all_device_layers_available(phy_device, layer_names, layer_names_count);
    EXPECT_SUCCESS(result);

    result = check_all_device_extensions_available(
        phy_device,
        extension_names,
        STATIC_ARRAY_SIZE(extension_names)
    );
    EXPECT_SUCCESS(result);

    fill_renderer_queues_create_info(queues, &queues_cis_count, queues_cis);

    device_ci.enabledLayerCount = layer_names_count;
    device_ci.ppEnabledLayerNames = layer_names;
    device_ci.enabledExtensionCount = STATIC_ARRAY_SIZE(extension_names);
    device_ci.ppEnabledExtensionNames = extension_names;
    device_ci.queueCreateInfoCount = queues_cis_count;
    device_ci.pQueueCreateInfos = queues_cis;

    result.error = vkCreateDevice(phy_device, &device_ci, NULL, &gpu);
    result.object = gpu;
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, "new GPU object created successfully");

failure:
    return result;
}

void fill_renderer_queues(Renderer renderer) {
    vkGetDeviceQueue(
        renderer->gpu,
        renderer->queues.graphics_family_idx,
        renderer->queues.graphics_local_idx,
        &renderer->queues.graphics
    );

    vkGetDeviceQueue(
        renderer->gpu,
        renderer->queues.present_family_idx,
        renderer->queues.present_local_idx,
        &renderer->queues.present
    );
}

Result new_command_pool(VkDevice gpu, uint32_t queue_family_index) {
    ASSERT_NOT_NULL(gpu);

    Result result = { 0 };

    VkCommandPoolCreateInfo cmd_pool_ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
    };
    VkCommandPool cmd_pool = VK_NULL_HANDLE;

    cmd_pool_ci.queueFamilyIndex = queue_family_index;

    result.error = vkCreateCommandPool(
        gpu,
        &cmd_pool_ci,
        NULL,
        &cmd_pool
    );
    result.object = cmd_pool;

    return result;
}

Apriori2Error new_renderer_command_pools(Renderer renderer) {
    ASSERT_NOT_NULL(renderer);

    Result result = { 0 };

    trace(LOG_TARGET, "creating new renderer command pools...");

    if (renderer->queues.graphics_family_idx == renderer->queues.present_family_idx) {
        uint32_t family_idx = renderer->queues.graphics_family_idx;

        result = new_command_pool(renderer->gpu, family_idx);
        RESULT_UNWRAP(
            renderer->pools.cmd.graphics,
            result
        );

        renderer->pools.cmd.present = renderer->pools.cmd.graphics;
    } else {
        result = new_command_pool(renderer->gpu, renderer->queues.graphics_family_idx);
        RESULT_UNWRAP(
            renderer->pools.cmd.graphics,
            result
        );

        result = new_command_pool(renderer->gpu, renderer->queues.present_family_idx);
        RESULT_UNWRAP(
            renderer->pools.cmd.present,
            result
        );
    }

    trace(LOG_TARGET, "new renderer command pools created successfully");

failure:
    return result.error;
}

Result new_command_buffer(VkDevice gpu, VkCommandPool cmd_pool, uint32_t buffer_count) {
    ASSERT_NOT_NULL(gpu);
    ASSERT_NOT_NULL(cmd_pool);

    Result result = { 0 };
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };
    VkCommandBuffer *buffers = calloc(buffer_count, sizeof(VkCommandBuffer));
    if (buffers == NULL) {
        result.error = OUT_OF_MEMORY;
        return result;
    }

    allocate_info.commandPool = cmd_pool;
    allocate_info.commandBufferCount = buffer_count;

    result.error = vkAllocateCommandBuffers(
        gpu,
        &allocate_info,
        buffers
    );
    result.object = buffers;

    return result;
}

Apriori2Error new_renderer_command_buffers(Renderer renderer) {
    ASSERT_NOT_NULL(renderer);

    Result result = { 0 };

    trace(LOG_TARGET, "creating new command buffers...");

    if (renderer->queues.graphics_family_idx == renderer->queues.present_family_idx) {
        result = new_command_buffer(
            renderer->gpu,
            renderer->pools.cmd.graphics,
            renderer->images_count
        );

        RESULT_UNWRAP(
            renderer->buffers.cmd.graphics,
            result
        );

        renderer->buffers.cmd.present = renderer->buffers.cmd.graphics;
    } else {
        result = new_command_buffer(
            renderer->gpu,
            renderer->pools.cmd.graphics,
            renderer->images_count
        );

        RESULT_UNWRAP(
            renderer->buffers.cmd.graphics,
            result
        );

        result = new_command_buffer(
            renderer->gpu,
            renderer->pools.cmd.present,
            renderer->images_count
        );

        RESULT_UNWRAP(
            renderer->buffers.cmd.present,
            result
        );
    }

    trace(LOG_TARGET, "new command buffers created successfully");

failure:
    return result.error;
}

Result new_images(Renderer renderer) {
    ASSERT_NOT_NULL(renderer);

    Result result = { 0 };
    uint32_t images_count = 0;
    DynArray images = NULL;

    trace(LOG_TARGET, "getting swapchain images");

    result.error = vkGetSwapchainImagesKHR(renderer->gpu, renderer->swapchain, &images_count, NULL);
    EXPECT_SUCCESS(result);

    result = NEW_DYN_ARRAY(images_count, sizeof(VkImage));
    RESULT_UNWRAP(images, result);

    result.error = vkGetSwapchainImagesKHR(
        renderer->gpu,
        renderer->swapchain,
        &images->count,
        AS(images->data, VkImage *)
    );
    EXPECT_SUCCESS(result);

    result.object = images;

    trace(LOG_TARGET, "swapchain images received successfully");
    return result;

failure:
    free(images);

    return result;
}

Result new_image_views(Renderer renderer) {
    ASSERT_NOT_NULL(renderer);

    Result result = { 0 };
    DynArray image_views = NULL;

    VkImageViewCreateInfo image_view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
    };
    VkComponentMapping components = {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    };
    VkImageSubresourceRange subresource_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseArrayLayer = 0,
        .baseMipLevel = 0,
        .layerCount = 1,
        .levelCount = 1
    };

    trace(LOG_TARGET, "creating new swapchain image views...");

    result = NEW_DYN_ARRAY(renderer->buffers.swapchain_images->count, VkImageView);
    RESULT_UNWRAP(image_views, result);

    image_view_ci.components = components;
    image_view_ci.subresourceRange = subresource_range;
    image_view_ci.format = renderer->surface_format.format;

    for (uint32_t i = 0; i < image_views->count; ++i) {
        image_view_ci.image = AS(renderer->buffers.swapchain_images->data, VkImage *)[i];

        result.error = vkCreateImageView(
            renderer->gpu,
            &image_view_ci,
            NULL,
            &AS(image_views->data, VkImageView *)[i]
        );
        EXPECT_SUCCESS(result);
    }

    result.object = image_views;
    trace(LOG_TARGET, "new swapchain image views created successfully");

    return result;

failure:
    free(image_views);
    return result;
}

Result new_renderer(
    VulkanInstance vulkan_instance,
    Handle window_platform_handle,
    uint16_t window_width,
    uint16_t window_height
) {
    Result result = { 0 };

    trace(
        LOG_TARGET,
        "creating new renderer..."
    );

    Renderer renderer = calloc(1, sizeof(struct RendererFFI));
    if (renderer == NULL) {
        result.error = OUT_OF_MEMORY;
        goto failure;
    }

    renderer->vk_instance = vulkan_instance;

    renderer->image_extent.width = window_width;
    renderer->image_extent.height = window_height;

    VkPhysicalDevice phy_device = select_phy_device(vulkan_instance);
    result = new_surface(vulkan_instance->vk_handle, window_platform_handle);
    RESULT_UNWRAP(
        renderer->surface,
        result
    );

    DynArray surface_formats = NULL;
    result = phy_device_surface_formats(phy_device, renderer->surface);
    RESULT_UNWRAP(
        surface_formats,
        result
    );

    renderer->surface_format = select_surface_format(surface_formats);

    free(surface_formats);

    result.error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        phy_device,
        renderer->surface,
        &renderer->surface_caps
    );
    EXPECT_SUCCESS(result);

    renderer->images_count = renderer->surface_caps.maxImageCount;
    trace(LOG_TARGET, "images count: %d", renderer->images_count);

    result.error = find_renderer_queue_family_indices(
        phy_device,
        &renderer->queues,
        renderer->surface
    );
    EXPECT_SUCCESS(result);

    result = new_gpu(phy_device, &renderer->queues);
    RESULT_UNWRAP(
        renderer->gpu,
        result
    );

    result = new_swapchain(phy_device, renderer);
    RESULT_UNWRAP(
        renderer->swapchain,
        result
    );

    result = new_images(renderer);
    RESULT_UNWRAP(
        renderer->buffers.swapchain_images,
        result
    );

    result = new_image_views(renderer);
    RESULT_UNWRAP(
        renderer->buffers.swapchain_views,
        result
    );

    fill_renderer_queues(renderer);

    result.error = new_renderer_command_pools(renderer);
    EXPECT_SUCCESS(result);

    result.error = new_renderer_command_buffers(renderer);
    EXPECT_SUCCESS(result);

    result.object = renderer;

    trace(
        LOG_TARGET,
        "new renderer successfully created"
    );

    return result;

failure:
    drop_renderer(result.object);

    return result;
}

void drop_renderer(Renderer renderer) {
    if (renderer == NULL)
        return;

    free(renderer->buffers.swapchain_views);
    free(renderer->buffers.swapchain_images);

    vkDestroySwapchainKHR(
        renderer->gpu,
        renderer->swapchain,
        NULL
    );

    if (renderer->queues.graphics_family_idx == renderer->queues.present_family_idx) {
        vkFreeCommandBuffers(
            renderer->gpu,
            renderer->pools.cmd.graphics,
            renderer->images_count,
            renderer->buffers.cmd.graphics
        );

        free(renderer->buffers.cmd.graphics);

        vkDestroyCommandPool(
            renderer->gpu,
            renderer->pools.cmd.graphics,
            NULL
        );
    } else {
        vkFreeCommandBuffers(
            renderer->gpu,
            renderer->pools.cmd.graphics,
            renderer->images_count,
            renderer->buffers.cmd.graphics
        );

        vkFreeCommandBuffers(
            renderer->gpu,
            renderer->pools.cmd.present,
            renderer->images_count,
            renderer->buffers.cmd.present
        );

        free(renderer->buffers.cmd.graphics);
        free(renderer->buffers.cmd.present);

        vkDestroyCommandPool(
            renderer->gpu,
            renderer->pools.cmd.graphics,
            NULL
        );

        vkDestroyCommandPool(
            renderer->gpu,
            renderer->pools.cmd.present,
            NULL
        );
    }

    vkDestroyDevice(renderer->gpu, NULL);

    drop_surface(renderer->vk_instance->vk_handle, renderer->surface);
    free(renderer);

    trace(LOG_TARGET, "drop renderer");
}