#ifndef ___APRIORI2_CORE_VULKAN_INSTANCE_IMPL_H___
#define ___APRIORI2_CORE_VULKAN_INSTANCE_IMPL_H___

#ifdef ___debug___
#   include "vk_debug_reporter.h"
#endif // ___debug___

struct VulkanInstanceFFI {
    VkInstance vk_handle;
    uint32_t phy_device_count;
    VkPhysicalDevice *phy_devices;

#ifdef ___debug___
    DebugReporter *dbg_reporter;
#endif // ___debug___
};

#endif // ___APRIORI2_CORE_VULKAN_INSTANCE_IMPL_H___