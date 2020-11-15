#ifndef ___APRIORI2_OS_WINDOWS_SURFACE_H___
#define ___APRIORI2_OS_WINDOWS_SURFACE_H___

#include <vulkan/vulkan.h>

#include "ffi/def.h"
#include "ffi/result.h"

Result new_surface(VkInstance instance, Handle window_platform_handle);

void drop_surface(VkInstance instance, VkSurfaceKHR surface);

#endif // ___APRIORI2_OS_WINDOWS_SURFACE_H___