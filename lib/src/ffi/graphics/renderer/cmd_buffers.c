#include "cmd_buffers.h"
#include "ffi/util/mod.h"
#include "ffi/core/log.h"

#define LOG_TARGET LOG_SUB_TARGET( \
    LOG_STRUCT_TARGET(Renderer), LOG_STRUCT_TARGET(RendererCmdBuffers) \
)

Result new_command_buffer(VkDevice device, VkCommandPool cmd_pool, uint32_t buffer_count) {
    ASSERT_NOT_NULL(device);
    ASSERT_NOT_NULL(cmd_pool);

    Result result = { 0 };
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };
    VkCommandBuffer *buffers = ALLOC_ARRAY(result, VkCommandBuffer, buffer_count);

    allocate_info.commandPool = cmd_pool;
    allocate_info.commandBufferCount = buffer_count;

    result.error = vkAllocateCommandBuffers(
        device,
        &allocate_info,
        buffers
    );
    result.object = buffers;

    FN_EXIT(result);

    FN_FAILURE(result, {
        free(buffers);
    });
}

Result new_renderer_cmd_buffers(
    VkDevice device,
    struct RendererQueueFamilies *queues,
    struct RendererCmdPools *cmd_pools,
    uint32_t buffers_count
) {
    ASSERT_NOT_NULL(device);
    ASSERT_NOT_NULL(queues);
    ASSERT_NOT_NULL(cmd_pools);

    Result result = { 0 };

    info(LOG_TARGET, "creating new command buffers...");
    struct RendererCmdBuffers *cmd_buffers = ALLOC(result, struct RendererCmdBuffers);

    cmd_buffers->device = device;
    cmd_buffers->buffers_count = buffers_count;

    if (is_same_queue_families(queues)) {
        result = new_command_buffer(
            device,
            cmd_pools->graphics,
            buffers_count
        );

        RESULT_UNWRAP(
            cmd_buffers->graphics,
            result
        );

        cmd_buffers->present = cmd_buffers->graphics;
    } else {
        result = new_command_buffer(
            device,
            cmd_pools->graphics,
            buffers_count
        );

        RESULT_UNWRAP(
            cmd_buffers->graphics,
            result
        );

        result = new_command_buffer(
            device,
            cmd_pools->present,
            buffers_count
        );

        RESULT_UNWRAP(
            cmd_buffers->present,
            result
        );
    }

    result.object = cmd_buffers;
    info(LOG_TARGET, "new command buffers created successfully");

    FN_EXIT(result);
    FN_FAILURE(result, {
        drop_renderer_cmd_buffers(cmd_buffers);
    });
}

void drop_renderer_cmd_buffers(struct RendererCmdBuffers *cmd_buffers) {
    if (cmd_buffers == NULL)
        goto exit;

    if (cmd_buffers->device && cmd_buffers->cmd_pools) {
        if (cmd_buffers->graphics) {
            vkFreeCommandBuffers(
                cmd_buffers->device,
                cmd_buffers->cmd_pools->graphics,
                cmd_buffers->buffers_count,
                cmd_buffers->graphics
            );

            free(cmd_buffers->graphics);
        }

        if (
            cmd_buffers->present
            && cmd_buffers->graphics != cmd_buffers->present
        ) {
            vkFreeCommandBuffers(
                cmd_buffers->device,
                cmd_buffers->cmd_pools->present,
                cmd_buffers->buffers_count,
                cmd_buffers->present
            );

            free(cmd_buffers->present);
        }
    }

    free(cmd_buffers);

exit:
    debug(LOG_TARGET, "drop renderer cmd buffers");
}