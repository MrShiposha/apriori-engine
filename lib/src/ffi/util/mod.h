#ifndef ___APRIORI2_UTIL_H___
#define ___APRIORI2_UTIL_H___

#include <assert.h>
#include <stdbool.h>

#include "dyn_array.h"

#define ASSERT_NOT_NULL(ptr) assert((ptr) != NULL && #ptr " must be not NULL")

#define IS_NULL(ptr) (ptr) == NULL

#define IS_NOT_NULL(ptr) (ptr) != NULL

#define UNWRAP_NULL(ptr) (ASSERT_NOT_NULL(ptr), ptr)

#define UNWRAP_NULL_OR(ptr, or_value) ((IS_NULL(ptr)) ? (or_value) : (*ptr))

#define STATIC_ARRAY_SIZE(array) \
    ((sizeof(array) / sizeof(array[0])) / ((size_t)(!(sizeof(array) % sizeof(array[0])))))

#define POW_2(pow, ty) (AS(1, ty) << (pow))

#define ___apriori_impl_MSB(value, check_pow_2, ...) \
    ((value) & POW_2(check_pow_2, uint32_t)) ? (check_pow_2) : (__VA_ARGS__)

#define MSB_32(value) \
    ___apriori_impl_MSB(value, 31, \
        ___apriori_impl_MSB(value, 30, \
       ___apriori_impl_MSB(value, 29, \
       ___apriori_impl_MSB(value, 28, \
       ___apriori_impl_MSB(value, 27, \
       ___apriori_impl_MSB(value, 26, \
       ___apriori_impl_MSB(value, 25, \
       ___apriori_impl_MSB(value, 24, \
       ___apriori_impl_MSB(value, 23, \
       ___apriori_impl_MSB(value, 22, \
       ___apriori_impl_MSB(value, 21, \
       ___apriori_impl_MSB(value, 20, \
       ___apriori_impl_MSB(value, 19, \
       ___apriori_impl_MSB(value, 18, \
       ___apriori_impl_MSB(value, 17, \
       ___apriori_impl_MSB(value, 16, \
       ___apriori_impl_MSB(value, 15, \
       ___apriori_impl_MSB(value, 14, \
       ___apriori_impl_MSB(value, 13, \
       ___apriori_impl_MSB(value, 12, \
       ___apriori_impl_MSB(value, 11, \
       ___apriori_impl_MSB(value, 10, \
       ___apriori_impl_MSB(value, 9, \
       ___apriori_impl_MSB(value, 8, \
       ___apriori_impl_MSB(value, 7, \
       ___apriori_impl_MSB(value, 6, \
       ___apriori_impl_MSB(value, 5, \
       ___apriori_impl_MSB(value, 4, \
       ___apriori_impl_MSB(value, 3, \
       ___apriori_impl_MSB(value, 2, \
       ___apriori_impl_MSB(value, 1, 0 \
    )))))))))))))))))))))))))))))))

#define IS_UINT32_POW_OF_2(value, ty) ((value) == POW_2(MSB_32(value), ty))

#define BYTES_TO_BITS(n) (n << 3)

#define N_SET_BITS(n, ty) ((AS(1, ty) << (n)) - 1)

#define N_RESET_BITS(n, ty) (~(N_SET_BITS(n, ty)))

#define AS_BINARY_COUNT_UNITS(value, count_units, ty) \
    ( \
        AS((value) + ((count_units) - 1), ty) \
        & ( \
            N_SET_BITS(BYTES_TO_BITS(sizeof(ty)) - 1, uint32_t) & N_RESET_BITS(MSB_32(count_units), uint32_t) \
        ) \
    ); \
    static_assert(sizeof(count_units) <= sizeof(uint32_t), "sizeof count units must be less of equal sizeof(uint32_t)"); \
    static_assert(IS_UINT32_POW_OF_2(count_units, uint32_t), "binary count units must be power of 2")

#endif // ___APRIORI2_UTIL_H___