#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "ffi/log.h"

#include "ffi/export/vulkan_instance.h"
#include "vulkan_instance.h"
#include "ffi/app_info.h"
#include "ffi/result_fns.h"

#include "ffi/util.h"
#include "ffi/def.h"

#ifdef ___debug___
    VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT object_type,
        uint64_t source_object,
        size_t location,
        int32_t message_code,
        const char *layer_prefix,
        const char *message,
        void *user_data
    ) {
        UNUSED_VAR(object_type);
        UNUSED_VAR(source_object);
        UNUSED_VAR(location);
        UNUSED_VAR(user_data);

        if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
            error("VULKAN", "%s: %s, code = %d", layer_prefix, message, message_code);
        } else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
            warn("VULKAN", "%s: %s, code = %d", layer_prefix, message, message_code);
        } else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
            info("VULKAN", "%s: %s, code = %d", layer_prefix, message, message_code);
        } else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
            warn("VULKAN", "%s: %s, code = %d", layer_prefix, message, message_code);
        } else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
            debug("VULKAN", "%s: %s, code = %d", layer_prefix, message, message_code);
        }

        // See PFN_vkDebugReportCallbackEXT in Vulkan spec.
        // Quote: The application should always return VK_FALSE.
        //        The VK_TRUE value is reserved for use in layer development.
        return VK_FALSE;
    }
#endif // ___debug___

#ifdef ___windows___
#   define VULKAN_PLATFORM_EXTENSION MACRO_EXPAND(VK_KHR_WIN32_SURFACE_EXTENSION_NAME)
#elif ___macos___
#   define VULKAN_PLATFORM_EXTENSION MACRO_EXPAND(VK_EXT_metal_surface)
#elif ___linux___
#   error "linux is not supported yet"
#elif ___unknown___
#   error "this target OS is not supported yet"
#endif // os

Result check_all_layers_available(const char **layers, uint32_t num_layers) {
    Apriori2Error err = SUCCESS;

    VkLayerProperties *layer_props = NULL;

    uint32_t property_count = 0;
    err = vkEnumerateInstanceLayerProperties(&property_count, NULL);
    if (err != VK_SUCCESS) {
        goto exit;
    }

    layer_props = malloc(property_count * sizeof(VkLayerProperties));
    if (layer_props == NULL) {
        err = OUT_OF_MEMORY;
        goto exit;
    }

    err = vkEnumerateInstanceLayerProperties(&property_count, layer_props);
    if (err != VK_SUCCESS)
        goto exit;

    for (uint32_t i = 0, j = 0; i < num_layers; ++i) {
        for (j = 0; j < property_count; ++j) {
            if (!strcmp(layers[i], layer_props[j].layerName))
                break;
        }

        // Some layer was not found
        if (j == property_count) {
            err = LAYERS_NOT_FOUND;
            error("Vulkan Instance", "Layer \"%s\" is not found", layers[i]);
        }
    }

exit:
    free(layer_props);
    return apriori2_error(err);
}

Result check_all_extensions_available(const char **extensions, uint32_t num_extensions) {
    Apriori2Error err = SUCCESS;
    VkExtensionProperties *extension_props = NULL;

    uint32_t property_count = 0;
    err = vkEnumerateInstanceExtensionProperties(NULL, &property_count, NULL);
    if (err != VK_SUCCESS) {
        goto exit;
    }

    extension_props = malloc(property_count * sizeof(VkLayerProperties));
    if (extension_props == NULL) {
        err = OUT_OF_MEMORY;
        goto exit;
    }

    err = vkEnumerateInstanceExtensionProperties(NULL, &property_count, extension_props);
    if (err != VK_SUCCESS)
        goto exit;

    for (uint32_t i = 0, j = 0; i < num_extensions; ++i) {
        for (j = 0; j < property_count; ++j) {
            if (!strcmp(extensions[i], extension_props[j].extensionName))
                break;
        }

        // Some extensions was not found
        if (j == property_count) {
            err = EXTENSIONS_NOT_FOUND;
            error("Vulkan Instance", "Extension \"%s\" is not found", extensions[i]);
        }
    }

exit:
    free(extension_props);
    return apriori2_error(err);
}

Result init_phy_devices(VulkanInstance instance) {
    Result result = { 0 };

    result.error = vkEnumeratePhysicalDevices(
        instance->vk_handle,
        &instance->phy_device_count,
        NULL
    );
    if (result.error != VK_SUCCESS)
        goto exit;

    instance->phy_devices = calloc(
        instance->phy_device_count,
        sizeof(VkPhysicalDevice)
    );
    if (instance->phy_devices == NULL) {
        result.error = OUT_OF_MEMORY;
        goto exit;
    }

    result.error = vkEnumeratePhysicalDevices(
        instance->vk_handle,
        &instance->phy_device_count,
        instance->phy_devices
    );

exit:
    return result;
}

Result new_vk_instance() {
    Result result = { 0 };

    VulkanInstance instance = calloc(1, sizeof(struct VulkanInstanceFFI));
    if (instance == NULL)
        return apriori2_error(OUT_OF_MEMORY);

    static VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = APRIORI2_APPLICATION_NAME,
        .applicationVersion = APRIORI2_VK_VERSION
    };

#   ifdef ___debug___
    const char *layer_names[] = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    const uint32_t layer_names_count = STATIC_ARRAY_SIZE(layer_names);
#   else
    const char *layer_names = NULL;
    const uint32_t layer_names_count = 0;
#   endif // ___debug___

    const char *extension_names[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VULKAN_PLATFORM_EXTENSION

#   ifdef ___debug___
        , VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#   endif // ___debug___
    };

    result = check_all_layers_available(
        layer_names,
        layer_names_count
    );
    if (result.error != SUCCESS)
        goto failure;

    result = check_all_extensions_available(
        extension_names,
        STATIC_ARRAY_SIZE(extension_names)
    );
    if (result.error != SUCCESS)
        goto failure;

    VkInstanceCreateInfo instance_ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = layer_names_count,
        .enabledExtensionCount = STATIC_ARRAY_SIZE(extension_names)
    };
    instance_ci.ppEnabledLayerNames = layer_names;
    instance_ci.ppEnabledExtensionNames = extension_names;

    result.error = vkCreateInstance(&instance_ci, NULL, &instance->vk_handle);
    if(result.error != VK_SUCCESS)
        goto failure;

    EXPECT_SUCCESS(
        init_phy_devices(instance)
    );

#   ifdef ___debug___
    result = new_debug_reporter(
        instance,
        debug_report
    );

    RESULT_UNWRAP(instance->dbg_reporter, result);
#   endif // ___debug___

    result.object = instance;
    return result;

failure:
    if (instance->vk_handle != NULL)
        drop_vk_instance(instance);
    else
        free(instance);

    error(
        "Vulkan Instance",
        "instance creation failed: error = %d",
        result.error
    );
    return result;
}

VkInstance vk_handle(VulkanInstance instance) {
    if (instance == NULL)
        return NULL;
    else
        return instance->vk_handle;
}

void drop_vk_instance(VulkanInstance instance) {
    if (instance == NULL)
        return;

#   ifdef ___debug___
    drop_debug_reporter(instance->dbg_reporter);
#   endif // ___debug___

    free(instance->phy_devices);

    vkDestroyInstance(instance->vk_handle, NULL);
    free(instance);
}