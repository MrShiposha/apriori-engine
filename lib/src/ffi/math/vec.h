#ifndef ___APRIORI2_MATH_VEC_H___
#define ___APRIORI2_MATH_VEC_H___

#include "math_def.h"

typedef struct {
    union {
        struct { Coord x, y; };
        struct { Coord u, v; };
    };
} Vec2;

typedef struct {
    union {
        struct { Coord x, y, z; };
        struct { Coord r, g, b; };
    };
} Vec3;

typedef struct {
    union {
        struct { Coord x, y, z, w; };
        struct { Coord r, g, b, a; };
    };
} Vec4;

#endif // ___APRIORI2_MATH_VEC_H___