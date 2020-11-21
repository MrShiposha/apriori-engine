#include "queues.h"
#include "ffi/util/mod.h"
#include "ffi/core/log.h"

#define LOG_TARGET LOG_SUB_TARGET( \
    LOG_STRUCT_TARGET(Renderer), LOG_STRUCT_TARGET(RendererQueues) \
)

Result new_renderer_queues(
    VkDevice device,
    struct RendererQueueFamilies *families,
    uint32_t queue_cis_count
) {
    Result result = { 0 };
    uint32_t graphics_idx = RENDERER_QUEUE_GRAPHICS_IDX;
    uint32_t present_idx = RENDERER_QUEUE_PRESENT_IDX;

    info(LOG_TARGET, "creating new renderer queues...");

    struct RendererQueues *queues = ALLOC(result, struct RendererQueues);

    if (queue_cis_count == 1) {
        present_idx = graphics_idx;
    }

    vkGetDeviceQueue(
        device,
        families->graphics_idx,
        graphics_idx,
        &queues->graphics
    );

    vkGetDeviceQueue(
        device,
        families->present_idx,
        present_idx,
        &queues->present
    );

    info(LOG_TARGET, "new renderer queues created successfully");

    FN_FORCE_EXIT(result);
}

void drop_renderer_queues(struct RendererQueues *queues) {
    if (queues == NULL)
        goto exit;

    free(queues);

exit:
    debug(LOG_TARGET, "drop renderer queues");
}