#ifndef ___APRIORI2_CORE_RESULT_H___
#define ___APRIORI2_CORE_RESULT_H___

#include "error.h"
#include "def.h"

#define RESULT_UNWRAP(out_object, result) do { \
    Result ___tmp_result = (result);\
    if (___tmp_result.error != SUCCESS) { \
        ___tmp_result.object = NULL; \
        (out_object) = NULL; \
        goto exit; \
    } \
    else (out_object) = ___tmp_result.object; \
} while(0)

#define EXPECT_SUCCESS(result) do { \
    if ((result).error != SUCCESS) \
        goto exit; \
} while(0)

#define EXPECT_MEM_ALLOC(result) do { \
    Result ___tmp_result = (result); \
    if (___tmp_result.object == NULL) { \
        ___tmp_result.error = OUT_OF_MEMORY; \
        goto exit; \
    } \
} while(0)

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