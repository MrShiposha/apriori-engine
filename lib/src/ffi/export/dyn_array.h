#ifndef ___APRIORI2_DYN_ARRAY_H___
#define ___APRIORI2_DYN_ARRAY_H___

#include <stdint.h>
#include <stdlib.h>

#include "def.h"
#include "result.h"

typedef struct {
    uint32_t count;
    Handle data;
} DynArrayT;

typedef DynArrayT *DynArray;

#define NEW_DYN_ARRAY(count, type) ___apriori_impl_new_dyn_array(count, sizeof(type))

Result ___apriori_impl_new_dyn_array(uint32_t count, size_t element_size);

#endif // ___APRIORI2_DYN_ARRAY_H___