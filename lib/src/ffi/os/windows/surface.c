#include <Windows.h>

#include "ffi/os/surface.h"

Result new_surface(
    VkInstance instance,
    Handle window_platform_handle
) {
    Result result = { 0 };

    VkWin32SurfaceCreateInfoKHR surface_ci = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR
    };

    surface_ci.hinstance = GetModuleHandle(NULL);
    surface_ci.hwnd = window_platform_handle;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    result.error = vkCreateWin32SurfaceKHR(
        instance,
        &surface_ci,
        NULL,
        &surface
    );
    result.object = surface;

    return result;
}

void drop_surface(VkInstance instance, VkSurfaceKHR surface) {
    vkDestroySurfaceKHR(instance, surface, NULL);
}
