#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "ffi/core/log.h"

#include "mod.h"
#include "ffi/core/vulkan_instance/vulkan_instance.impl.h"
#include "ffi/core/app_info.h"

#include "ffi/util/mod.h"
#include "ffi/core/def.h"

#define LOG_TARGET LOG_STRUCT_TARGET(VulkanInstance)

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
            error("Vulkan", "%s: %s, code = %d", layer_prefix, message, message_code);
        } else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
            warn("Vulkan", "%s: %s, code = %d", layer_prefix, message, message_code);
        } else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
            info("Vulkan", "%s: %s, code = %d", layer_prefix, message, message_code);
        } else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
            warn("Vulkan", "%s: %s, code = %d", layer_prefix, message, message_code);
        } else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
            debug("Vulkan", "%s: %s, code = %d", layer_prefix, message, message_code);
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
    Result result = { 0 };
    VkLayerProperties *layer_props = NULL;
    uint32_t property_count = 0;

    trace(LOG_TARGET, LOG_GROUP(struct, "checking requested validation layers"));

    result.error = vkEnumerateInstanceLayerProperties(&property_count, NULL);
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, LOG_GROUP(struct, "available validation layers count: %d"), property_count);

    layer_props = ALLOC_ARRAY_UNINIT(result, VkLayerProperties, property_count);

    result.error = vkEnumerateInstanceLayerProperties(&property_count, layer_props);
    EXPECT_SUCCESS(result);

    for (uint32_t i = 0, j = 0; i < num_layers; ++i) {
        for (j = 0; j < property_count; ++j) {
            if (!strcmp(layers[i], layer_props[j].layerName))
                break;
        }

        if (j == property_count) {
            // Some layer was not found
            result.error = LAYERS_NOT_FOUND;
            error(LOG_TARGET, "layer \"%s\" is not found", layers[i]);
        } else {
            trace(LOG_TARGET, LOG_GROUP(struct_op, "validation layer \"%s\": OK"), layers[i]);
        }
    }

    FN_FORCE_EXIT(result, {
        free(layer_props);
    });
}

Result check_all_extensions_available(const char **extensions, uint32_t num_extensions) {
    Result result = { 0 };
    VkExtensionProperties *extension_props = NULL;
    uint32_t property_count = 0;

    trace(LOG_TARGET, LOG_GROUP(struct, "checking requested extensions"));

    result.error = vkEnumerateInstanceExtensionProperties(NULL, &property_count, NULL);
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, LOG_GROUP(struct, "available extension count: %d"), property_count);

    extension_props = ALLOC_ARRAY_UNINIT(result, VkLayerProperties, property_count);

    result.error = vkEnumerateInstanceExtensionProperties(NULL, &property_count, extension_props);
    EXPECT_SUCCESS(result);

    for (uint32_t i = 0, j = 0; i < num_extensions; ++i) {
        for (j = 0; j < property_count; ++j) {
            if (!strcmp(extensions[i], extension_props[j].extensionName))
                break;
        }

        if (j == property_count) {
            // Some extensions was not found
            result.error = EXTENSIONS_NOT_FOUND;
            error(LOG_TARGET, "extension \"%s\" is not found", extensions[i]);
        } else {
            trace(LOG_TARGET, LOG_GROUP(struct_op, "extension \"%s\": OK"), extensions[i]);
        }
    }

    FN_FORCE_EXIT(result, {
        free(extension_props);
    });
}

Result init_phy_devices(VulkanInstance instance) {
    Result result = { 0 };

    trace(LOG_TARGET, LOG_GROUP(struct, "initializing physical devices..."));

    result.error = vkEnumeratePhysicalDevices(
        instance->vk_handle,
        &instance->phy_device_count,
        NULL
    );
    EXPECT_SUCCESS(result);

    instance->phy_devices = ALLOC_ARRAY(result, VkPhysicalDevice, instance->phy_device_count);

    result.error = vkEnumeratePhysicalDevices(
        instance->vk_handle,
        &instance->phy_device_count,
        instance->phy_devices
    );
    EXPECT_SUCCESS(result);

    trace(LOG_TARGET, LOG_GROUP(struct, "physical devices successfully initialized"));

    FN_FORCE_EXIT(result);
}

Result new_vk_instance() {
    Result result = { 0 };

    info(LOG_TARGET, "creating new vulkan instance...");

    VulkanInstance instance = ALLOC(result, struct VulkanInstanceFFI);

    static VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = APRIORI2_APPLICATION_NAME,
        .applicationVersion = APRIORI2_VK_VERSION
    };

    trace(
        LOG_TARGET,
        LOG_GROUP(struct, "application name: %s, application version: %d"),
        app_info.pApplicationName, app_info.applicationVersion
    );

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

    result = init_phy_devices(instance);
    EXPECT_SUCCESS(result);

#   ifdef ___debug___
    result = new_debug_reporter(
        instance,
        debug_report
    );

    RESULT_UNWRAP(instance->dbg_reporter, result);
#   endif // ___debug___

    result.object = instance;
    info(LOG_TARGET, "new vulkan instance created successfully");

    FN_EXIT(result);

    FN_FAILURE(result, {
        drop_vk_instance(instance);

        error(
            LOG_TARGET,
            "instance creation failed: error = %d",
            result.error
        );
    });
}

VkInstance vk_handle(VulkanInstance instance) {
    if (instance == NULL)
        return NULL;
    else
        return instance->vk_handle;
}

void drop_vk_instance(VulkanInstance instance) {
    if (instance == NULL)
        goto exit;

#   ifdef ___debug___
    drop_debug_reporter(instance->dbg_reporter);
#   endif // ___debug___

    free(instance->phy_devices);

    vkDestroyInstance(instance->vk_handle, NULL);
    free(instance);

exit:
    debug(LOG_TARGET, "drop vulkan instance");
}