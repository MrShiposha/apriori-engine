#ifndef ___APRIORI2_EXPORT_RENDERER_H___
#define ___APRIORI2_EXPORT_RENDERER_H___

#include "ffi/result.h"
#include "vulkan_instance.h"

typedef struct RendererFFI *Renderer;

Result new_renderer(
    VulkanInstance vulkan_instance,
    Handle window_platform_handle
);

void drop_renderer(Renderer renderer);

#endif // ___APRIORI2_EXPORT_RENDERER_H___