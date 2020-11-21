#ifndef ___APRIORI2_CORE_VULKAN_INSTANCE_DEBUG_REPORTER_H___
#define ___APRIORI2_CORE_VULKAN_INSTANCE_DEBUG_REPORTER_H___

#include <vulkan/vulkan.h>
#include "ffi/core/result.h"
#include "mod.h"

typedef struct DebugReporter {
    VulkanInstance instance;
    VkDebugReportCallbackEXT callback;
} DebugReporter;

Result new_debug_reporter(VulkanInstance instance, PFN_vkDebugReportCallbackEXT callback);

void drop_debug_reporter(DebugReporter *debug_reporter);

#endif // ___APRIORI2_CORE_VULKAN_INSTANCE_DEBUG_REPORTER_H___