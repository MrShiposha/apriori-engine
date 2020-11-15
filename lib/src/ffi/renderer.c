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

#define LOG_TARGET "FFI/Renderer"
#define CI_GRAPHICS_IDX 0
#define CI_PRESENT_IDX 1

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

Apriori2Error find_renderer_queue_family_indices(
    VkPhysicalDevice phy_device,
    struct RendererQueues *queues,
    VkSurfaceKHR surface
) {
    assert(queues != NULL && "queues must be not NULL");

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
    assert(queues != NULL && "queues must be not NULL");
    assert(queues_cis_count != NULL && "queues_cis_count must be not NULL");
    assert(queues_cis != NULL && "queues_cis must be not NULL");

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
    assert(phy_device != NULL && "phy_device must be not NULL");
    assert(queues != NULL && "queues must be not NULL");

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
    assert(renderer != NULL && "pools must be not NULL");

    Result result = { 0 };

    trace(LOG_TARGET, "creating new renderer command pools...");

    if (renderer->queues.graphics_family_idx == renderer->queues.present_family_idx) {
        uint32_t family_idx = renderer->queues.graphics_family_idx;

        result = new_command_pool(renderer->gpu, family_idx);
        RESULT_UNWRAP(
            renderer->pools.graphics_cmd,
            result
        );

        renderer->pools.present_cmd = renderer->pools.graphics_cmd;
    } else {
        result = new_command_pool(renderer->gpu, renderer->queues.graphics_family_idx);
        RESULT_UNWRAP(
            renderer->pools.graphics_cmd,
            result
        );

        result = new_command_pool(renderer->gpu, renderer->queues.present_family_idx);
        RESULT_UNWRAP(
            renderer->pools.present_cmd,
            result
        );
    }

    trace(LOG_TARGET, "new renderer command pools created successfully");

failure:
    return result.error;
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

    Renderer renderer = calloc(1, sizeof(struct RendererFFI));
    if (renderer == NULL) {
        result.error = OUT_OF_MEMORY;
        goto failure;
    }

    renderer->vk_instance = vulkan_instance;

    VkPhysicalDevice phy_device = select_phy_device(vulkan_instance);
    result = new_surface(vulkan_instance->vk_handle, window_platform_handle);
    RESULT_UNWRAP(
        renderer->surface,
        result
    );

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

    fill_renderer_queues(renderer);

    result.error = new_renderer_command_pools(renderer);
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

    vkDestroyDevice(renderer->gpu, NULL);

    drop_surface(renderer->vk_instance->vk_handle, renderer->surface);
    free(renderer);

    trace(LOG_TARGET, "drop renderer");
}