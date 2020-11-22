#include "mod.h"

#include <string.h>

Result ___apriori_impl_new_dyn_array(uint32_t count, size_t element_size) {
    Result result = { 0 };

    DynArray array = ALLOC_ARRAY(result, Byte, sizeof(DynArrayT) + element_size * count);

    array->count = count;
    array->data = AS(array, Bytes) + sizeof(DynArrayT);

    FN_FORCE_EXIT(result);
}

Result ___apriori_impl_combine_dyn_arrays(DynArray arrays, uint32_t arrays_count, size_t element_size) {
    ASSERT_NOT_NULL(arrays);

    Result result = { 0 };
    DynArray combined_array = NULL;
    Bytes data = NULL;
    uint32_t combined_count = 0;
    size_t combined_size = 0;

    for (uint32_t i = 0; i < arrays_count; ++i)
        combined_count += arrays[i].count;

    combined_size = combined_count * element_size;

    result = ___apriori_impl_new_dyn_array(combined_count, element_size);
    RESULT_UNWRAP(combined_array, result);

    data = combined_array->data;

    for (uint32_t i = 0; i < arrays_count; ++i) {
        errno_t copy_result = memcpy_s(
            data,
            combined_size,
            arrays[i].data,
            arrays[i].count
        );

        assert(copy_result == 0 && "unable to combine dyn arrays");

        data += element_size;
        combined_size -= element_size;
    }

    FN_EXIT(result);

    FN_FAILURE(result, {
        free(combined_array);
    });
}

Result ___apriori_impl_new_array_from_object(void *object, uint32_t count, size_t element_size) {
    ASSERT_NOT_NULL(object);

    Result result = { 0 };
    DynArray array = NULL;
    Bytes data = NULL;
    size_t data_size = count * element_size;

    result = ___apriori_impl_new_dyn_array(count, element_size);
    EXPECT_SUCCESS(result);

    array = result.object;
    data = array->data;

    for (uint32_t i = 0; i < count; ++i) {
        errno_t copy_error = memcpy_s(
            data,
            data_size,
            object,
            element_size
        );

        assert(copy_error == 0 && "unable to copy object into array");

        data += element_size;
        data_size -= element_size;
    }

    FN_EXIT(result);

    FN_FAILURE(result, {
        free(array);
    });
}