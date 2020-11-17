#include "export/dyn_array.h"

Result ___apriori_impl_new_dyn_array(uint32_t count, size_t element_size) {
    Result result = { 0 };

    DynArray array = calloc(1, sizeof(DynArrayT) + element_size * count);
    EXPECT_MEM_ALLOC(result, array);

    array->count = count;
    array->data = AS(array, Bytes) + sizeof(DynArrayT);

    result.object = array;

failure:
    return result;
}