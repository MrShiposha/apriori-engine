#ifndef ___APRIORI2_RESULT_H___
#define ___APRIORI2_RESULT_H___

#include "error.h"
#include "def.h"

#define RESULT_UNWRAP(out_object, result) do { \
    Result ___tmp_result = (result);\
    if (___tmp_result.error != SUCCESS) { \
        ___tmp_result.object = NULL; \
        (out_object) = NULL; \
        goto failure; \
    } \
    else (out_object) = ___tmp_result.object; \
} while(0)

#define EXPECT_SUCCESS(result) do { \
    if ((result).error != SUCCESS) \
        goto failure; \
} while(0)

typedef struct Result {
    Apriori2Error error;
    Handle object;
} Result;

#endif // ___APRIORI2_RESULT_H___