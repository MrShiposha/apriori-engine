#ifndef ___APRIORI2_CORE_ERROR_H___
#define ___APRIORI2_CORE_ERROR_H___

#include <stdint.h>

#include <vulkan/vulkan.h>

#define APRIORI2_ERROR_NUM (1000)

// When adding a new case, do NOT forget add it to `error_to_string` fn.
typedef enum Apriori2Error {
    SUCCESS = VK_SUCCESS,
    OUT_OF_MEMORY = -APRIORI2_ERROR_NUM,
    VK_PROC_NOT_FOUND,
    DEBUG_REPORTER_CREATION,
    LAYERS_NOT_FOUND,
    EXTENSIONS_NOT_FOUND,
    GRAPHICS_QUEUE_FAMILY_NOT_FOUND,
    PRESENT_QUEUE_FAMILY_NOT_FOUND,
    RENDERER_QUEUE_FAMILIES_NOT_FOUND
} Apriori2Error;

const char *error_to_string(Apriori2Error error);

#endif // ___APRIORI2_CORE_ERROR_H___