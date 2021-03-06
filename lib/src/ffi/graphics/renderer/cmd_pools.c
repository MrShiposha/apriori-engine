#include "cmd_pools.h"
#include "ffi/util/mod.h"
#include "ffi/core/log.h"

#define LOG_TARGET LOG_SUB_TARGET( \
    LOG_STRUCT_TARGET(Renderer), LOG_STRUCT_TARGET(RendererCmdPools) \
)

Result new_command_pool(VkDevice device, uint32_t queue_family_index) {
    ASSERT_NOT_NULL(device);

    Result result = { 0 };

    VkCommandPoolCreateInfo cmd_pool_ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
    };
    VkCommandPool cmd_pool = VK_NULL_HANDLE;

    cmd_pool_ci.queueFamilyIndex = queue_family_index;

    result.error = vkCreateCommandPool(
        device,
        &cmd_pool_ci,
        NULL,
        &cmd_pool
    );
    result.object = cmd_pool;

    return result;
}

Result new_renderer_cmd_pools(VkDevice device, struct RendererQueueFamilies *queues) {
    ASSERT_NOT_NULL(device);
    ASSERT_NOT_NULL(queues);

    Result result = { 0 };

    info(LOG_TARGET, "creating new renderer command pools...");

    struct RendererCmdPools *cmd_pools = ALLOC(result, struct RendererCmdPools);

    cmd_pools->device = device;

    if (is_same_queue_families(queues)) {
        uint32_t family_idx = queues->graphics_idx;

        result = new_command_pool(device, family_idx);
        RESULT_UNWRAP(
            cmd_pools->graphics,
            result
        );

        cmd_pools->present = cmd_pools->graphics;
    } else {
        result = new_command_pool(device, queues->graphics_idx);
        RESULT_UNWRAP(
            cmd_pools->graphics,
            result
        );

        result = new_command_pool(device, queues->present_idx);
        RESULT_UNWRAP(
            cmd_pools->present,
            result
        );
    }

    result.object = cmd_pools;
    info(LOG_TARGET, "new renderer command pools created successfully");

    FN_EXIT(result);

    FN_FAILURE(result, {
        drop_renderer_cmd_pools(cmd_pools);
    });
}

void drop_renderer_cmd_pools(struct RendererCmdPools *cmd_pools) {
    if (cmd_pools == NULL)
        goto exit;

    if (cmd_pools->device) {
        if (cmd_pools->graphics)
            vkDestroyCommandPool(cmd_pools->device, cmd_pools->graphics, NULL);

        if (cmd_pools->present && cmd_pools->graphics != cmd_pools->present)
            vkDestroyCommandPool(cmd_pools->device, cmd_pools->present, NULL);
    }

    free(cmd_pools);

exit:
    debug(LOG_TARGET, "drop renderer cmd pools");
}