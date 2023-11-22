#pragma once

#include <stddef.h>

inline void *memcpy(void *dest, const void *src, size_t size) {
    asm ("rep movsb" :: "D"(dest), "S"(src), "c"(size) : "memory");

    return dest;
}

inline void *memset(void *dest, int c, size_t size) {
    asm ("rep stosb" :: "D"(dest), "a"(c), "c"(size) : "memory");

    return dest;
}
