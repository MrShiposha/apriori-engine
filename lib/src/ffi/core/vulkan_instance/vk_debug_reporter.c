#include <assert.h>
#include <stdlib.h>

#include "vk_debug_reporter.h"
#include "vulkan_instance.impl.h"

#include "ffi/core/log.h"

#define LOG_TARGET "FFI/DebugReporter"

Result new_debug_reporter(VulkanInstance instance, PFN_vkDebugReportCallbackEXT callback) {
    Result result = { 0 };
    DebugReporter *reporter = NULL;

    trace(
        LOG_TARGET,
        "creating new debug reporter..."
    );

    PFN_vkCreateDebugReportCallbackEXT
    vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
        vk_handle(instance),
        "vkCreateDebugReportCallbackEXT"
    );
    result.object = (Handle)vkCreateDebugReportCallbackEXT;
    EXPECT_MEM_ALLOC(result);

    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .pfnCallback = callback,
        .flags = VK_DEBUG_REPORT_WARNING_BIT_EXT
                | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
                | VK_DEBUG_REPORT_ERROR_BIT_EXT
                | VK_DEBUG_REPORT_DEBUG_BIT_EXT
    };

    reporter = malloc(sizeof(DebugReporter));
    result.object = reporter;
    EXPECT_MEM_ALLOC(result);

    reporter->instance = instance;
    result.error = vkCreateDebugReportCallbackEXT(
        vk_handle(instance),
        &debug_report_ci,
        NULL,
        &reporter->callback
    );

    FN_EXIT(result);

    FN_FAILURE(result, {
        free(reporter);
    });
}

void drop_debug_reporter(DebugReporter *debug_reporter) {
    if (debug_reporter == NULL)
        return;

    PFN_vkDestroyDebugReportCallbackEXT
    vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
        vk_handle(debug_reporter->instance),
        "vkDestroyDebugReportCallbackEXT"
    );

    if (vkDestroyDebugReportCallbackEXT != NULL) {
        vkDestroyDebugReportCallbackEXT(
            vk_handle(debug_reporter->instance),
            debug_reporter->callback,
            NULL
        );
    }

    trace(
        LOG_TARGET,
        "drop debug reporter"
    );
}