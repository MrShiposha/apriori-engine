#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>

#include "mod.h"
#include "renderer.impl.h"

#include "ffi/core/def.h"
#include "ffi/core/error.h"
#include "ffi/core/vulkan_instance/vulkan_instance.impl.h"
#include "ffi/core/log.h"
#include "ffi/os/surface.h"
#include "ffi/util/mod.h"

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

    FN_EXIT(result);

    FN_FAILURE(result, {
        free(surface_formats);
    });
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

void fill_renderer_queues_create_info(
    struct RendererQueueFamilies *families,
    VkDeviceQueueCreateInfo *queues_cis,
    uint32_t *queues_cis_count
) {
    ASSERT_NOT_NULL(families);
    ASSERT_NOT_NULL(queues_cis_count);
    ASSERT_NOT_NULL(queues_cis);

    static float priorities[2] = { 0 };

    trace(LOG_TARGET, "filling renderer queues create infos...");

    priorities[CI_GRAPHICS_IDX] = 1.0f;
    priorities[CI_PRESENT_IDX] = 0.75f;

    if (is_same_queue_families(families)) {
        uint32_t family_idx = families->graphics_idx;

        *queues_cis_count = 1;

        queues_cis->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queues_cis->queueCount = 2;
        queues_cis->queueFamilyIndex = family_idx;
        queues_cis->pQueuePriorities = priorities;
    } else {
        *queues_cis_count = 2;

        queues_cis[CI_GRAPHICS_IDX].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queues_cis[CI_GRAPHICS_IDX].queueCount = 1;
        queues_cis[CI_GRAPHICS_IDX].queueFamilyIndex = families->graphics_idx;
        queues_cis[CI_GRAPHICS_IDX].pQueuePriorities = &priorities[CI_GRAPHICS_IDX];

        queues_cis[CI_PRESENT_IDX].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queues_cis[CI_PRESENT_IDX].queueCount = 1;
        queues_cis[CI_PRESENT_IDX].queueFamilyIndex = families->present_idx;
        queues_cis[CI_PRESENT_IDX].pQueuePriorities = &priorities[CI_PRESENT_IDX];
    }
}

Result check_all_device_layers_available(VkPhysicalDevice device, const char **layers, uint32_t num_layers) {
    Result result = { 0 };
    VkLayerProperties *layer_props = NULL;
    uint32_t property_count = 0;

    trace(LOG_TARGET, "checking requested validation layers");

    result.error = vkEnumerateDeviceLayerProperties(device, &property_count, NULL);
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, "available validation layers count: %d", property_count);

    layer_props = ALLOC_ARRAY_UNINIT(result, VkLayerProperties, property_count);

    result.error = vkEnumerateDeviceLayerProperties(device, &property_count, layer_props);
    EXPECT_SUCCESS(result);

    for (uint32_t i = 0, j = 0; i < num_layers; ++i) {
        for (j = 0; j < property_count; ++j) {
            if (!strcmp(layers[i], layer_props[j].layerName))
                break;
        }

        if (j == property_count) {
            // Some layer was not found
            result.error = LAYERS_NOT_FOUND;
            error(LOG_TARGET, "layer \"%s\" is not found", layers[i]);
        } else {
            trace(LOG_TARGET, "\tvalidation layer \"%s\": OK", layers[i]);
        }
    }

    FN_FORCE_EXIT(result, {
        free(layer_props);
    });
}

Result check_all_device_extensions_available(
    VkPhysicalDevice phy_device,
    const char **extensions,
    uint32_t num_extensions
) {
    Result result = { 0 };
    VkExtensionProperties *extension_props = NULL;
    uint32_t property_count = 0;

    trace(LOG_TARGET, "checking requested extensions");

    result.error = vkEnumerateDeviceExtensionProperties(phy_device, NULL, &property_count, NULL);
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, "available extension count: %d", property_count);

    extension_props = ALLOC_ARRAY_UNINIT(result, VkLayerProperties, property_count);

    result.error = vkEnumerateDeviceExtensionProperties(phy_device, NULL, &property_count, extension_props);
    EXPECT_SUCCESS(result);

    for (uint32_t i = 0, j = 0; i < num_extensions; ++i) {
        for (j = 0; j < property_count; ++j) {
            if (!strcmp(extensions[i], extension_props[j].extensionName))
                break;
        }

        if (j == property_count) {
            // Some extensions was not found
            result.error = EXTENSIONS_NOT_FOUND;
            error(LOG_TARGET, "extension \"%s\" is not found", extensions[i]);
        } else {
            trace(LOG_TARGET, "\textension \"%s\": OK", extensions[i]);
        }
    }

    FN_FORCE_EXIT(result, {
        free(extension_props);
    });
}

Result new_gpu(
    VkPhysicalDevice phy_device,
    struct RendererQueueFamilies *families,
    VkDeviceQueueCreateInfo *queues_cis,
    uint32_t queues_cis_count
) {
    ASSERT_NOT_NULL(phy_device);
    ASSERT_NOT_NULL(families);
    ASSERT_NOT_NULL(queues_cis);

    Result result = { 0 };
    VkDeviceCreateInfo device_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
    };
    VkDevice gpu = VK_NULL_HANDLE;

    info(LOG_TARGET, "creating new GPU object...");

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


    device_ci.enabledLayerCount = layer_names_count;
    device_ci.ppEnabledLayerNames = layer_names;
    device_ci.enabledExtensionCount = STATIC_ARRAY_SIZE(extension_names);
    device_ci.ppEnabledExtensionNames = extension_names;
    device_ci.queueCreateInfoCount = queues_cis_count;
    device_ci.pQueueCreateInfos = queues_cis;

    result.error = vkCreateDevice(phy_device, &device_ci, NULL, &gpu);
    result.object = gpu;
    EXPECT_SUCCESS(result);

    info(LOG_TARGET, "new GPU object created successfully");

    FN_FORCE_EXIT(result);
}

Result new_renderer(
    VulkanInstance vulkan_instance,
    Handle window_platform_handle,
    uint16_t window_width,
    uint16_t window_height
) {
    Result result = { 0 };
    struct RendererQueueFamilies *families = NULL;
    uint32_t queues_cis_count = 0;
    VkDeviceQueueCreateInfo queues_cis[2] = { 0 };
    DynArray surface_formats = NULL;
    struct SwapchainCreateParams swapchain_params = { 0 };

    info(
        LOG_TARGET,
        "creating new renderer..."
    );

    Renderer renderer = ALLOC(result, struct RendererFFI);

    renderer->vk_instance = vulkan_instance;

    VkPhysicalDevice phy_device = select_phy_device(vulkan_instance);
    result = new_surface(vulkan_instance->vk_handle, window_platform_handle);
    RESULT_UNWRAP(
        renderer->surface,
        result
    );

    result = new_renderer_queue_families(phy_device, renderer->surface);
    RESULT_UNWRAP(
        families,
        result
    );

    fill_renderer_queues_create_info(families, queues_cis, &queues_cis_count);

    result = new_gpu(phy_device, families, queues_cis, queues_cis_count);
    RESULT_UNWRAP(
        renderer->gpu,
        result
    );

    result = new_renderer_queues(renderer->gpu, families, queues_cis_count);
    RESULT_UNWRAP(
        renderer->queues,
        result
    );

    swapchain_params.phy_device = phy_device;
    swapchain_params.device = renderer->gpu;
    swapchain_params.image_extent.width = window_width;
    swapchain_params.image_extent.height = window_height;
    swapchain_params.families = families;
    swapchain_params.surface = renderer->surface;

    result = phy_device_surface_formats(phy_device, renderer->surface);
    RESULT_UNWRAP(
        surface_formats,
        result
    );

    swapchain_params.surface_format = select_surface_format(surface_formats);

    result = new_swapchain(&swapchain_params);
    RESULT_UNWRAP(
        renderer->swapchain,
        result
    );

    result = new_renderer_cmd_pools(renderer->gpu, families);
    RESULT_UNWRAP(
        renderer->pools.cmd,
        result
    );

    result = new_renderer_cmd_buffers(
        renderer->gpu,
        families,
        renderer->pools.cmd,
        renderer->swapchain->image_count
    );
    RESULT_UNWRAP(
        renderer->buffers.cmd,
        result
    );

    result.object = renderer;

    trace(
        LOG_TARGET,
        "new renderer successfully created"
    );

    FN_EXIT(result, {
        drop_renderer_queue_families(families);
        free(surface_formats);
    });

    FN_FAILURE(result, {
        drop_renderer(renderer);
    });
}

void drop_renderer(Renderer renderer) {
    if (renderer == NULL)
        goto exit;

    drop_renderer_cmd_buffers(renderer->buffers.cmd);

    drop_renderer_cmd_pools(renderer->pools.cmd);

    drop_swapchain(renderer->swapchain);

    vkDestroyDevice(renderer->gpu, NULL);

    drop_surface(renderer->vk_instance->vk_handle, renderer->surface);

    free(renderer);

exit:
    debug(LOG_TARGET, "drop renderer");
}