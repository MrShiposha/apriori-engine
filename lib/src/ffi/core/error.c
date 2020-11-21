#include "ffi/core/error.h"

const char *error_to_string(Apriori2Error error) {
    uint32_t general_error = (uint32_t)error;

#define APRIORI_CASE(v, ...) case v: return #v __VA_ARGS__
#define VK_CASE(v, ...) case VK_##v: return "(Vulkan API) " #v __VA_ARGS__

    switch (general_error) {
    APRIORI_CASE(SUCCESS);
    APRIORI_CASE(OUT_OF_MEMORY, ": memory allocation failure");
    APRIORI_CASE(VK_PROC_NOT_FOUND, ": vkGetInstanceProcAddr failed");
    APRIORI_CASE(DEBUG_REPORTER_CREATION, ": unable to create Vulkan Instance debug reporter");
    APRIORI_CASE(LAYERS_NOT_FOUND, ": some Vulkan validation layers was not found");
    APRIORI_CASE(EXTENSIONS_NOT_FOUND, ": some Vulkan extensions was not found");
    APRIORI_CASE(GRAPHICS_QUEUE_FAMILY_NOT_FOUND, ": graphics queue family was not found on the physical device");
    APRIORI_CASE(PRESENT_QUEUE_FAMILY_NOT_FOUND, ": present queue family was not found on the physical device");
    APRIORI_CASE(
        RENDERER_QUEUE_FAMILIES_NOT_FOUND,
        ": both graphics and present queue families were not found on the physical device"
    );

    VK_CASE(NOT_READY);
    VK_CASE(TIMEOUT);
    VK_CASE(EVENT_SET);
    VK_CASE(EVENT_RESET);
    VK_CASE(INCOMPLETE);
    VK_CASE(ERROR_OUT_OF_HOST_MEMORY);
    VK_CASE(ERROR_OUT_OF_DEVICE_MEMORY);
    VK_CASE(ERROR_INITIALIZATION_FAILED);
    VK_CASE(ERROR_DEVICE_LOST);
    VK_CASE(ERROR_MEMORY_MAP_FAILED);
    VK_CASE(ERROR_LAYER_NOT_PRESENT);
    VK_CASE(ERROR_EXTENSION_NOT_PRESENT);
    VK_CASE(ERROR_FEATURE_NOT_PRESENT);
    VK_CASE(ERROR_INCOMPATIBLE_DRIVER);
    VK_CASE(ERROR_TOO_MANY_OBJECTS);
    VK_CASE(ERROR_FORMAT_NOT_SUPPORTED);
    VK_CASE(ERROR_SURFACE_LOST_KHR);
    VK_CASE(ERROR_NATIVE_WINDOW_IN_USE_KHR);
    VK_CASE(SUBOPTIMAL_KHR);
    VK_CASE(ERROR_OUT_OF_DATE_KHR);
    VK_CASE(ERROR_INCOMPATIBLE_DISPLAY_KHR);
    VK_CASE(ERROR_VALIDATION_FAILED_EXT);
    VK_CASE(ERROR_INVALID_SHADER_NV);
    default:
        return "Unknown error";
    }

#undef VK_CASE
#undef APRIORI_CASE
}