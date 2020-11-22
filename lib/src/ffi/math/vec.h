#ifndef ___APRIORI2_MATH_VEC_H___
#define ___APRIORI2_MATH_VEC_H___


typedef struct {
    union {
        struct { float x, y; };
        struct { float u, v; };
    };
} float2;

typedef struct {
    union {
        struct { float x, y, z; };
        struct { float r, g, b; };
    };
} float3;

typedef struct {
    union {
        struct { float x, y, z, w; };
        struct { float r, g, b, a; };
    };
} float4;

#endif // ___APRIORI2_MATH_VEC_H___