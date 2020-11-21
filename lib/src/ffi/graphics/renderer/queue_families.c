#include "queues.h"

#include "ffi/core/log.h"
#include "ffi/util/mod.h"

#define LOG_TARGET LOG_SUB_TARGET( \
    LOG_STRUCT_TARGET(Renderer), LOG_STRUCT_TARGET(RendererQueueFamilies) \
)

Result new_renderer_queue_families(
    VkPhysicalDevice phy_device,
    VkSurfaceKHR surface
) {
    ASSERT_NOT_NULL(phy_device);
    ASSERT_NOT_NULL(surface);

    Result result = { 0 };
    VkQueueFamilyProperties *family_props = NULL;

    info(
        LOG_TARGET,
        "creating new renderer queue families..."
    );

    struct RendererQueueFamilies *families = ALLOC(result, struct RendererQueueFamilies);

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        phy_device,
        &queue_family_count,
        NULL
    );

    trace(
        LOG_TARGET,
        LOG_GROUP(struct, "queue family count: %d"),
        queue_family_count
    );

    family_props = ALLOC_ARRAY_UNINIT(result, VkQueueFamilyProperties, queue_family_count);

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

        result.error = vkGetPhysicalDeviceSurfaceSupportKHR(
            phy_device,
            i,
            surface,
            &is_present_support
        );
        EXPECT_SUCCESS(result);

        if (
            (current->queueFlags & VK_QUEUE_GRAPHICS_BIT)
            && is_present_support
        ) {
            families->graphics_idx = i;
            families->present_idx = i;

            is_graphics_queue_found = true;
            is_present_queue_found = true;
            break;
        }

        if (current->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            families->graphics_idx = i;
            is_graphics_queue_found = true;
        }

        if (is_present_support) {
            families->present_idx = i;
            is_present_queue_found = true;
        }
    }

    if (!is_graphics_queue_found && !is_present_queue_found)
        result.error = RENDERER_QUEUE_FAMILIES_NOT_FOUND;
    else if (!is_graphics_queue_found)
        result.error = GRAPHICS_QUEUE_FAMILY_NOT_FOUND;
    else if (!is_present_queue_found)
        result.error = PRESENT_QUEUE_FAMILY_NOT_FOUND;
    else {
        result.object = families;

        info(
            LOG_TARGET,
            "new renderer queue families created successfully"
        );

        trace(
            LOG_TARGET,
            LOG_GROUP(struct, "graphics queue family idx: %d"),
            families->graphics_idx
        );

        trace(
            LOG_TARGET,
            LOG_GROUP(struct, "present queue family idx: %d"),
            families->present_idx
        );
    }

    FN_EXIT(result, {
        free(family_props);
    });

    FN_FAILURE(result, {
        free(family_props);
    });
}

bool is_same_queue_families(struct RendererQueueFamilies *families) {
    ASSERT_NOT_NULL(families);

    return families->graphics_idx == families->present_idx;
}

void drop_renderer_queue_families(struct RendererQueueFamilies *queues) {
    if (queues == NULL)
        goto exit;

    free(queues);

exit:
    debug(LOG_TARGET, "drop renderer queue families");
}