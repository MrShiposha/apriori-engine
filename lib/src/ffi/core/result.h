#ifndef ___APRIORI2_CORE_RESULT_H___
#define ___APRIORI2_CORE_RESULT_H___

#include <stdlib.h>

#include "error.h"
#include "def.h"

// Sets error to result and attachs error description
#define ___apriori_impl_NORMALIZE_RESULT(result, error_value) \
    ((result).error = (error_value), (result).object = (Handle)error_to_string((error_value)))

#define RESULT_UNWRAP(out_object, result) do { \
    Result ___tmp_result = (result);\
    if (___tmp_result.error != SUCCESS) { \
        ___apriori_impl_NORMALIZE_RESULT(result, ___tmp_result.error); \
        (out_object) = NULL; \
        goto exit; \
    } \
    else (out_object) = ___tmp_result.object; \
} while(0)

#define EXPECT_SUCCESS(result) do { \
    Result ___tmp_result = (result); \
    if (___tmp_result.error != SUCCESS) {\
        ___apriori_impl_NORMALIZE_RESULT(result, ___tmp_result.error); \
        goto exit; \
    } \
} while(0)

#define UNWRAP_NOT_NULL(result, error_value, ptr) \
    (((ptr) == NULL) ? \
        (___apriori_impl_NORMALIZE_RESULT(result, error_value), NULL) \
        : ((result).error = SUCCESS, (result).object = (ptr)) \
    ); EXPECT_SUCCESS(result)

#define ALLOC_WITH(result, alloc_fn, ...) \
    UNWRAP_NOT_NULL(result, OUT_OF_MEMORY, (alloc_fn)(__VA_ARGS__))

#define ALLOC_ARRAY(result, ty, count) ALLOC_WITH(result, calloc, count, sizeof(ty))

#define ALLOC(result, ty) ALLOC_ARRAY(result, ty, 1)

#define ALLOC_ARRAY_UNINIT(result, ty, count) ALLOC_WITH(result, malloc, count * sizeof(ty))

#define ALLOC_UNINIT(result, ty) ALLOC_ARRAY_UNINIT(result, ty, 1)

#define FN_EXIT(result, ...) do { \
exit: \
    __VA_ARGS__ \
    Result ___tmp_result = (result); \
    if (___tmp_result.error != SUCCESS) { \
        ___tmp_result.object = NULL; \
        goto failure; \
    } else { \
        return ___tmp_result; \
    } \
} while(0)

#define FN_FORCE_EXIT(result, ...) do { \
exit: \
    __VA_ARGS__ \
    return (result); \
} while(0)

#define FN_FAILURE(result, ...) do { \
failure: \
    Result ___tmp_result = (result); \
    __VA_ARGS__ \
    return ___tmp_result; \
} while(0)

typedef struct Result {
    Apriori2Error error;
    Handle object;
} Result;

#endif // ___APRIORI2_CORE_RESULT_H___