#ifndef ___APRIORI2_UTIL_H___
#define ___APRIORI2_UTIL_H___

#define STATIC_ARRAY_SIZE(array) \
    ((sizeof(array) / sizeof(array[0])) / ((size_t)(!(sizeof(array) % sizeof(array[0])))))

#endif // ___APRIORI2_UTIL_H___