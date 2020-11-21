#include "dyn_array.h"

Result ___apriori_impl_new_dyn_array(uint32_t count, size_t element_size) {
    Result result = { 0 };

    DynArray array = ALLOC_ARRAY(result, Byte, sizeof(DynArrayT) + element_size * count);

    array->count = count;
    array->data = AS(array, Bytes) + sizeof(DynArrayT);

    FN_FORCE_EXIT(result);
}