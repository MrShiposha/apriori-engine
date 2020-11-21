#ifndef ___APRIORI2_UTIL_H___
#define ___APRIORI2_UTIL_H___

#include <assert.h>

#include "dyn_array.h"

#define ASSERT_NOT_NULL(value) assert((value) != NULL && #value " must be not NULL")

#define STATIC_ARRAY_SIZE(array) \
    ((sizeof(array) / sizeof(array[0])) / ((size_t)(!(sizeof(array) % sizeof(array[0])))))

#endif // ___APRIORI2_UTIL_H___