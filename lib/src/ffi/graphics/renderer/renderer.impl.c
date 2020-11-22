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

#define LOG_TARGET LOG_STRUCT_TARGET(Renderer)

#define CI_GRAPHICS_IDX 0
#define CI_PRESENT_IDX 1

struct PhyDeviceDescr {
    VkPhysicalDevice phy_device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
};

uint32_t rate_phy_device_suitability(
    VkPhysicalDeviceProperties *dev_props,
    VkPhysicalDeviceFeatures *dev_features
) {
    ASSERT_NOT_NULL(dev_props);
    ASSERT_NOT_NULL(dev_features);

    uint32_t score = 0;

#define DEV_TYPE_CASE(dev_type, add_score) \
    case VK_PHYSICAL_DEVICE_TYPE_##dev_type: \
        score += add_score; \
        trace( \
            LOG_TARGET, \
            LOG_GROUP(struct_op, "\"%s\" device type: " #dev_type), \
            dev_props->deviceName \
        ); \
        break

    switch(dev_props->deviceType) {
        DEV_TYPE_CASE(DISCRETE_GPU, 1000);
        DEV_TYPE_CASE(INTEGRATED_GPU, 100);
        DEV_TYPE_CASE(VIRTUAL_GPU, 10);
    }
#undef DEV_TYPE_CASE

#define DEV_HAS_FEATURE(feature_name, add_score) do {\
    if (dev_features->feature_name) { \
        trace( \
            LOG_TARGET, \
            LOG_GROUP(struct_op, "\"%s\" has '" #feature_name "' feature"), \
            dev_props->deviceName \
        ); \
        score += add_score; \
    } \
} while(0)

    DEV_HAS_FEATURE(samplerAnisotropy, 10);

#undef DEV_HAS_FEATURE
    trace(
        LOG_TARGET,
        LOG_GROUP(struct, "\"%s\" physical device suitability score: %d"),
        dev_props->deviceName, score
    );

    return score;
}

Result select_phy_device(VulkanInstance instance) {
    ASSERT_NOT_NULL(instance);

    Result result = { 0 };
    struct PhyDeviceDescr *winner_descr = NULL;

    uint32_t score = 0;
    uint32_t current_score = 0;
    VkPhysicalDeviceProperties dev_props = { 0 };
    VkPhysicalDeviceFeatures dev_features = { 0 };

    trace(
        LOG_TARGET,
        LOG_GROUP(struct, "selecting the most suitable physical device...")
    );

    winner_descr = ALLOC(result, struct PhyDeviceDescr);

    for (uint32_t i = 0; i < instance->phy_device_count; ++i) {
        vkGetPhysicalDeviceProperties(instance->phy_devices[i], &dev_props);
        vkGetPhysicalDeviceFeatures(instance->phy_devices[i], &dev_features);

        current_score = rate_phy_device_suitability(
            &dev_props,
            &dev_features
        );

        if (current_score > score) {
            score = current_score;
            winner_descr->phy_device = instance->phy_devices[i];
            winner_descr->properties = dev_props;
            winner_descr->features = dev_features;
        }
    }

    assert(
        winner_descr->phy_device != VK_NULL_HANDLE
        && "Renderer: physical device must be selected"
    );

    trace(
        LOG_TARGET,
        LOG_GROUP(struct, "the most suitable physical device: \"%s\" (score: %d)"),
        winner_descr->properties.deviceName, score
    );

    FN_FORCE_EXIT(result);
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

    result = NEW_DYN_ARRAY(VkSurfaceFormatKHR, surface_formats_count);
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
            trace(LOG_TARGET, LOG_GROUP(struct, "found SRGB B8G8R8A8 format"));
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

    trace(LOG_TARGET, LOG_GROUP(struct, "filling renderer queues create infos..."));

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

    trace(LOG_TARGET, LOG_GROUP(struct, "checking requested validation layers"));

    result.error = vkEnumerateDeviceLayerProperties(device, &property_count, NULL);
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, LOG_GROUP(struct, "available validation layers count: %d"), property_count);

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
            trace(LOG_TARGET, LOG_GROUP(struct_op, "validation layer \"%s\": OK"), layers[i]);
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

    trace(LOG_TARGET, LOG_GROUP(struct, "checking requested extensions"));

    result.error = vkEnumerateDeviceExtensionProperties(phy_device, NULL, &property_count, NULL);
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, LOG_GROUP(struct, "available extension count: %d"), property_count);

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
            trace(LOG_TARGET, LOG_GROUP(struct_op, "extension \"%s\": OK"), extensions[i]);
        }
    }

    FN_FORCE_EXIT(result, {
        free(extension_props);
    });
}

Result new_gpu(
    struct PhyDeviceDescr *phy_dev_descr,
    struct RendererQueueFamilies *families,
    VkDeviceQueueCreateInfo *queues_cis,
    uint32_t queues_cis_count
) {
    ASSERT_NOT_NULL(phy_dev_descr);
    ASSERT_NOT_NULL(families);
    ASSERT_NOT_NULL(queues_cis);

    Result result = { 0 };
    VkPhysicalDeviceFeatures enabled_features = { 0 };
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

    result = check_all_device_layers_available(phy_dev_descr->phy_device, layer_names, layer_names_count);
    EXPECT_SUCCESS(result);

    result = check_all_device_extensions_available(
        phy_dev_descr->phy_device,
        extension_names,
        STATIC_ARRAY_SIZE(extension_names)
    );
    EXPECT_SUCCESS(result);

    if (phy_dev_descr->features.samplerAnisotropy)
        enabled_features.samplerAnisotropy = VK_TRUE;

    device_ci.enabledLayerCount = layer_names_count;
    device_ci.ppEnabledLayerNames = layer_names;
    device_ci.enabledExtensionCount = STATIC_ARRAY_SIZE(extension_names);
    device_ci.ppEnabledExtensionNames = extension_names;
    device_ci.queueCreateInfoCount = queues_cis_count;
    device_ci.pQueueCreateInfos = queues_cis;
    device_ci.pEnabledFeatures = &enabled_features;

    result.error = vkCreateDevice(phy_dev_descr->phy_device, &device_ci, NULL, &gpu);
    result.object = gpu;
    EXPECT_SUCCESS(result);

    info(LOG_TARGET, "new GPU object created successfully");

    FN_FORCE_EXIT(result);
}

Result new_render_pass(VkDevice device, VkFormat surface_format) {
    ASSERT_NOT_NULL(device);

    Result result = { 0 };
    VkAttachmentDescription color_attachment = {
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    color_attachment.format = surface_format;

    const uint32_t color_attachment_idx = 0;
    VkAttachmentReference color_attachment_ref = {
        .attachment = color_attachment_idx,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpasses[] = {
        {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
        }
    };
    subpasses[RENDER_SUBPASS_OVERLAY_IDX].pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .subpassCount = STATIC_ARRAY_SIZE(subpasses),

        // TODO subpasses dependencies
        .dependencyCount = 0,
        .pDependencies = NULL
    };
    VkRenderPass render_pass = VK_NULL_HANDLE;

    render_pass_ci.pAttachments = &color_attachment;
    render_pass_ci.pSubpasses = subpasses;

    info(LOG_TARGET, LOG_GROUP(struct, "creating new renderer render pass..."));

    result.error = vkCreateRenderPass(
        device,
        &render_pass_ci,
        NULL,
        &render_pass
    );
    result.object = render_pass;
    EXPECT_SUCCESS(result);

    info(LOG_TARGET, LOG_GROUP(struct, "new renderer render pass created successfully"));

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

    struct PhyDeviceDescr *phy_dev_descr = NULL;
    result = select_phy_device(vulkan_instance);
    RESULT_UNWRAP(phy_dev_descr, result);

    VkPhysicalDevice phy_device = phy_dev_descr->phy_device;
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

    result = new_gpu(phy_dev_descr, families, queues_cis, queues_cis_count);
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

    result = get_ovl_descriptor_pool_sizes(renderer->swapchain->image_count);
    EXPECT_SUCCESS(result);

    // TODO Combine different pool size arrays
    DynArray descr_pool_sizes = result.object;

    uint32_t max_sets_count = renderer->swapchain->image_count;

    info(LOG_TARGET, LOG_GROUP(struct, "creating new renderer descriptor pool..."));
    {
        VkDescriptorPoolCreateInfo descr_pool_ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = max_sets_count,
        };
        descr_pool_ci.poolSizeCount = descr_pool_sizes->count;
        descr_pool_ci.pPoolSizes = descr_pool_sizes->data;

        result.error = vkCreateDescriptorPool(
            renderer->gpu,
            &descr_pool_ci,
            NULL,
            &renderer->pools.descr
        );
        EXPECT_SUCCESS(result);
    }
    info(LOG_TARGET, LOG_GROUP(struct, "new renderer descriptor pool created sucessfully"));

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

    result = new_render_pass(
        renderer->gpu,
        swapchain_params.surface_format.format
    );
    RESULT_UNWRAP(
        renderer->render_pass,
        result
    );

    result = new_pipeline_ovl(
        renderer->gpu,
        renderer->render_pass,
        &swapchain_params.image_extent,
        renderer->swapchain->image_count,
        NULL
    );
    RESULT_UNWRAP(
        renderer->pipelines.overlay,
        result
    );

    result.object = renderer;

    info(
        LOG_TARGET,
        "new renderer created successfully"
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

    VkResult result = vkDeviceWaitIdle(renderer->gpu);
    if (result != VK_SUCCESS)
        error(LOG_TARGET, "unable to wait device idle");

    drop_pipeline_ovl(renderer->pipelines.overlay);

    vkDestroyRenderPass(
        renderer->gpu,
        renderer->render_pass,
        NULL
    );
    debug(LOG_TARGET, LOG_GROUP(struct, "drop renderer render pass"));

    vkDestroyDescriptorPool(
        renderer->gpu,
        renderer->pools.descr,
        NULL
    );
    debug(LOG_TARGET, LOG_GROUP(struct, "drop renderer descriptor pool"));

    drop_renderer_cmd_buffers(renderer->buffers.cmd);

    drop_renderer_cmd_pools(renderer->pools.cmd);

    drop_swapchain(renderer->swapchain);

    vkDestroyDevice(renderer->gpu, NULL);
    debug(LOG_TARGET, LOG_GROUP(struct, "drop GPU object"));

    drop_surface(renderer->vk_instance->vk_handle, renderer->surface);

    free(renderer);

exit:
    debug(LOG_TARGET, "drop renderer");
}