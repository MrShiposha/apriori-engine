#ifndef ___APRIORI2_UTIL_DYN_ARRAY_H___
#define ___APRIORI2_UTIL_DYN_ARRAY_H___

#include <stdint.h>
#include <stdlib.h>

#include "ffi/core/def.h"
#include "ffi/core/result.h"

typedef struct {
    uint32_t count;
    Handle data;
} DynArrayT;

typedef DynArrayT *DynArray;

#define NEW_DYN_ARRAY(type, count) ___apriori_impl_new_dyn_array((count), sizeof(type))

#define COMBINE_DYN_ARRAYS(arrays, type, count) ___apriori_impl_combine_dyn_arrays((arrays), (count), sizeof(type))

#define NEW_DYN_ARRAY_FROM_OBJECT(object, count) ___apriori_impl_new_array_from_object((object), (count), sizeof((object)))

Result ___apriori_impl_new_dyn_array(uint32_t count, size_t element_size);

Result ___apriori_impl_combine_dyn_arrays(DynArray arrays, uint32_t arrays_count, size_t element_size);

Result ___apriori_impl_new_array_from_object(void *object, uint32_t count, size_t element_size);

#endif // ___APRIORI2_DYN_ARRAY_H___