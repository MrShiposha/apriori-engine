#include <assert.h>
#include <stdlib.h>

#include "vk_debug_reporter.h"
#include "vulkan_instance.impl.h"

#include "ffi/core/log.h"

#define LOG_TARGET LOG_STRUCT_TARGET(DebugReporter)

Result new_debug_reporter(VulkanInstance instance, PFN_vkDebugReportCallbackEXT callback) {
    Result result = { 0 };
    DebugReporter *reporter = NULL;

    info(
        LOG_TARGET,
        "creating new debug reporter..."
    );

    PFN_vkCreateDebugReportCallbackEXT
    vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
        vk_handle(instance),
        "vkCreateDebugReportCallbackEXT"
    );
    UNWRAP_NOT_NULL(result, VK_PROC_NOT_FOUND, (Handle)vkCreateDebugReportCallbackEXT);

    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .pfnCallback = callback,
        .flags = VK_DEBUG_REPORT_WARNING_BIT_EXT
                | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
                | VK_DEBUG_REPORT_ERROR_BIT_EXT
                | VK_DEBUG_REPORT_DEBUG_BIT_EXT
    };

    reporter = ALLOC_UNINIT(result, DebugReporter);

    reporter->instance = instance;
    result.error = vkCreateDebugReportCallbackEXT(
        vk_handle(instance),
        &debug_report_ci,
        NULL,
        &reporter->callback
    );

    info(LOG_TARGET, "new debug reporter created successfully");

    FN_EXIT(result);

    FN_FAILURE(result, {
        free(reporter);
    });
}

void drop_debug_reporter(DebugReporter *debug_reporter) {
    if (debug_reporter == NULL)
        goto exit;

    if (debug_reporter->instance == NULL)
        goto exit;

    if (vk_handle(debug_reporter->instance) == VK_NULL_HANDLE)
        goto exit;

    PFN_vkDestroyDebugReportCallbackEXT
    vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
        vk_handle(debug_reporter->instance),
        "vkDestroyDebugReportCallbackEXT"
    );

    if (debug_reporter->callback && vkDestroyDebugReportCallbackEXT != NULL) {
        vkDestroyDebugReportCallbackEXT(
            vk_handle(debug_reporter->instance),
            debug_reporter->callback,
            NULL
        );
    }

exit:
    debug(
        LOG_TARGET,
        "drop debug reporter"
    );
}