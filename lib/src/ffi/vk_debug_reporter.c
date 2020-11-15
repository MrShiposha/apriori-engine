#include <assert.h>
#include <stdlib.h>

#include "vk_debug_reporter.h"
#include "log.h"
#include "result_fns.h"

#define LOG_TARGET "FFI/DebugReporter"

Result new_debug_reporter(VulkanInstance instance, PFN_vkDebugReportCallbackEXT callback) {
    trace(
        LOG_TARGET,
        "creating new debug reporter..."
    );

    PFN_vkCreateDebugReportCallbackEXT
    vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
        vk_handle(instance),
        "vkCreateDebugReportCallbackEXT"
    );

    if (vkCreateDebugReportCallbackEXT == NULL)
        return apriori2_error(DEBUG_REPORTER_CREATION);

    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .pfnCallback = callback,
        .flags = VK_DEBUG_REPORT_WARNING_BIT_EXT
                | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
                | VK_DEBUG_REPORT_ERROR_BIT_EXT
                | VK_DEBUG_REPORT_DEBUG_BIT_EXT
    };

    DebugReporter *reporter = malloc(sizeof(DebugReporter));
    if (reporter == NULL)
        return apriori2_error(OUT_OF_MEMORY);

    reporter->instance = instance;
    VkResult result = vkCreateDebugReportCallbackEXT(
        vk_handle(instance),
        &debug_report_ci,
        NULL,
        &reporter->callback
    );

    if (result == VK_SUCCESS) {
        trace(
            LOG_TARGET,
            "new debug reporter successfully created"
        );
    }

    return new_result(reporter, result);
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
        "drop debug reporter..."
    );
}