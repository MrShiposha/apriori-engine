#ifndef ___APRIORI2_CORE_DEF_H___
#define ___APRIORI2_CORE_DEF_H___

typedef void *Handle;

typedef unsigned char Byte;

typedef Byte *Bytes;

#define MACRO_EXPAND(...) __VA_ARGS__

#define UNUSED_VAR(x) (void)(x)

#define AS(x, ty) ((ty)(x))

#endif // ___APRIORI2_CORE_DEF_H___