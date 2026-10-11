#ifndef __LIMITS_H
#define __LIMITS_H

// ACR-7000: 9-bit chars, 18-bit shorts and 36-bit ints, longs and long
// longs. Plain char is unsigned.

#define CHAR_BIT 9
#define MB_LEN_MAX 4

#define SCHAR_MAX 255
#define SCHAR_MIN (-256)
#define UCHAR_MAX 511
#define CHAR_MAX UCHAR_MAX
#define CHAR_MIN 0

#define SHRT_MAX 131071
#define SHRT_MIN (-131072)
#define USHRT_MAX 262143

#define INT_MAX 34359738367
#define INT_MIN (-INT_MAX - 1)
#define UINT_MAX 68719476735U

#define LONG_MAX 34359738367L
#define LONG_MIN (-LONG_MAX - 1L)
#define ULONG_MAX 68719476735UL

#define LLONG_MAX 34359738367LL
#define LLONG_MIN (-LLONG_MAX - 1LL)
#define ULLONG_MAX 68719476735ULL

#endif
