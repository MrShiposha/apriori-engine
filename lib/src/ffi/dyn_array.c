#include "export/dyn_array.h"

Result ___apriori_impl_new_dyn_array(uint32_t count, size_t element_size) {
    Result result = { 0 };

    DynArray array = calloc(1, sizeof(DynArrayT) + element_size * count);
    result.object = array;
    EXPECT_MEM_ALLOC(result);

    array->count = count;
    array->data = AS(array, Bytes) + sizeof(DynArrayT);

    FN_FORCE_EXIT(result);
}