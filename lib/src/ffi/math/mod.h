#ifndef ___APRIORI2_MATH_H___
#define ___APRIORI2_MATH_H___

#include "ffi/core/def.h"
#include "math_def.h"
#include "vec.h"

#define CEIL_32(value) (((AS((value), float) - AS((value), int32_t)) == 0) ? AS((value), int32_t) : AS((value), int32_t) + 1)
#define FLOOR_32(value) AS((value), int32_t)

#endif // ___APRIORI2_MATH_H___