#ifndef LIMITS_H
#define LIMITS_H

#define PAGE_SIZE 4096

#define CHAR_MIN (-128)
#define CHAR_MAX 127
#define CHAR_BIT 8
#define SCHAR_MIN (-128)
#define SCHAR_MAX 127
#define UCHAR_MAX 255
#define SHRT_MIN  (-1-0x7fff)
#define SHRT_MAX  0x7fff
#define USHRT_MAX 0xffff
#define INT_MIN  (-1-0x7fffffff)
#define INT_MAX  0x7fffffff
#define UINT_MAX 0xffffffffU
#define LONG_MAX  0x7fffffffL
#define LONG_MIN (-LONG_MAX-1)
#define LLONG_MAX  0x7fffffffffffffffLL
#define LLONG_MIN (-LLONG_MAX-1)
#define ULONG_MAX (0xffffffffL)
#define ULLONG_MAX (0xffffffffffffffffULL)

#define PATH_MAX 4096

#endif