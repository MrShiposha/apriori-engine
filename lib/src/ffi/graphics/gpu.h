#ifndef ___APRIORI2_GRAPHICS_GPU_H___
#define ___APRIORI2_GRAPHICS_GPU_H___

#ifdef ___gpu___
#   define vk_location(loc) [[vk::location(loc)]]
#   define semantics(sem) : sem
#else
#   include "ffi/math/mod.h"

#   define SHADER_DEFAULT_ENTRY_POINT "main"

#   define vk_location(_)
#   define semantics(_)
#endif // ___gpu___

#endif // ___APRIORI2_GRAPHICS_GPU_H___