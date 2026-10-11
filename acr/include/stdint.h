#ifndef __STDINT_H
#define __STDINT_H

// ACR-7000: chars are 9 bits, shorts 18 and ints and longs 36, so none
// of the optional exact-width types (int8_t, uint32_t, ...) exist.

typedef signed char int_least8_t;
typedef short int_least16_t;
typedef int int_least32_t;
typedef unsigned char uint_least8_t;
typedef unsigned short uint_least16_t;
typedef unsigned int uint_least32_t;

typedef int int_fast8_t;
typedef int int_fast16_t;
typedef int int_fast32_t;
typedef unsigned int uint_fast8_t;
typedef unsigned int uint_fast16_t;
typedef unsigned int uint_fast32_t;

typedef long intptr_t;
typedef unsigned long uintptr_t;
typedef long intmax_t;
typedef unsigned long uintmax_t;

#define INT_LEAST8_MIN (-256)
#define INT_LEAST8_MAX 255
#define UINT_LEAST8_MAX 511
#define INT_LEAST16_MIN (-131072)
#define INT_LEAST16_MAX 131071
#define UINT_LEAST16_MAX 262143
#define INT_LEAST32_MIN (-34359738367 - 1)
#define INT_LEAST32_MAX 34359738367
#define UINT_LEAST32_MAX 68719476735U

#define INT_FAST8_MIN INT_LEAST32_MIN
#define INT_FAST8_MAX INT_LEAST32_MAX
#define UINT_FAST8_MAX UINT_LEAST32_MAX
#define INT_FAST16_MIN INT_LEAST32_MIN
#define INT_FAST16_MAX INT_LEAST32_MAX
#define UINT_FAST16_MAX UINT_LEAST32_MAX
#define INT_FAST32_MIN INT_LEAST32_MIN
#define INT_FAST32_MAX INT_LEAST32_MAX
#define UINT_FAST32_MAX UINT_LEAST32_MAX

#define INTPTR_MIN (-34359738367L - 1)
#define INTPTR_MAX 34359738367L
#define UINTPTR_MAX 68719476735UL
#define INTMAX_MIN (-34359738367L - 1)
#define INTMAX_MAX 34359738367L
#define UINTMAX_MAX 68719476735UL
#define PTRDIFF_MIN (-34359738367L - 1)
#define PTRDIFF_MAX 34359738367L
#define SIZE_MAX 68719476735UL

#define INT8_C(x) (x)
#define INT16_C(x) (x)
#define INT32_C(x) (x)
#define UINT8_C(x) (x ## U)
#define UINT16_C(x) (x ## U)
#define UINT32_C(x) (x ## U)
#define INTMAX_C(x) (x ## L)
#define UINTMAX_C(x) (x ## UL)

#endif
