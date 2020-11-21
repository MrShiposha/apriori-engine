#include <Windows.h>

#include "ffi/os/surface.h"
#include "ffi/core/log.h"

#define LOG_TARGET LOG_SUB_TARGET( \
    LOG_STRUCT_TARGET(Renderer), LOG_STRUCT_TARGET(Surface) \
)

Result new_surface(
    VkInstance instance,
    Handle window_platform_handle
) {
    Result result = { 0 };

    VkWin32SurfaceCreateInfoKHR surface_ci = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR
    };

    info(LOG_TARGET, "creating new renderer surface...");

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
    EXPECT_SUCCESS(result);

    info(LOG_TARGET, "new renderer surface created successfully");

    FN_FORCE_EXIT(result);
}

void drop_surface(VkInstance instance, VkSurfaceKHR surface) {
    vkDestroySurfaceKHR(instance, surface, NULL);

    debug(LOG_TARGET, "drop surface");
}
