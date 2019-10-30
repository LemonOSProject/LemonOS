#ifndef STDINT_H
#define STDINT_H

#ifdef Lemon32
#define _int64 long long
#else
#define _int64 long
#endif

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed _int64 int64_t;
typedef signed _int64 intmax_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned _int64 uint64_t;
typedef unsigned _int64 uintmax_t;

#ifdef Lemon32
typedef uint32_t uintptr_t;
#else
typedef uint64_t uintptr_t;
#endif

#endif