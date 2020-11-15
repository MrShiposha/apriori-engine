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

#define LOG_TARGET "FFI/Renderer"

uint32_t rate_phy_device_suitability(VkPhysicalDeviceProperties *dev_props) {
    assert(dev_props != NULL && "dev_props must be not NULL");

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

Apriori2Error init_renderer_queues(
    struct RendererQueues *queues,
    VkPhysicalDevice device,
    VkSurfaceKHR surface
) {
    Apriori2Error error = SUCCESS;

    trace(
        LOG_TARGET,
        "initializing renderer queues..."
    );

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        device,
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
        device,
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
            device,
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
            queues->graphics_idx = i;
            queues->present_idx = i;

            is_graphics_queue_found = true;
            is_present_queue_found = true;
            break;
        }

        if (current->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queues->graphics_idx = i;
            is_graphics_queue_found = true;
        }

        if (is_present_support) {
            queues->present_idx = i;
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
            queues->graphics_idx
        );

        trace(
            LOG_TARGET,
            "\tpresent queue family idx: %d",
            queues->present_idx
        );
    }

    return error;
}

Result new_renderer(
    VulkanInstance vulkan_instance,
    Handle window_platform_handle
) {
    Result result = { 0 };

    trace(
        LOG_TARGET,
        "creating new renderer..."
    );

    result.object = calloc(1, sizeof(struct RendererFFI));
    if (result.object == NULL) {
        result.error = OUT_OF_MEMORY;
        goto failure;
    }

    Renderer renderer = AS(result.object, Renderer);
    renderer->vk_instance = vulkan_instance;

    VkPhysicalDevice phy_device = select_phy_device(vulkan_instance);
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    RESULT_UNWRAP(
        surface,
        new_surface(vulkan_instance->vk_handle, window_platform_handle)
    );

    result.error = init_renderer_queues(
        &renderer->queues,
        phy_device,
        surface
    );
    EXPECT_SUCCESS(result);

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

    free(renderer);

    trace(LOG_TARGET, "drop renderer");
}