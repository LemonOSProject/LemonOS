#pragma once

#include <stddef.h>

extern "C" {

inline void *memcpy(void *dest, const void *src, size_t size) {
    asm ("rep movsb" :: "D"(dest), "S"(src), "c"(size) : "memory");

    return dest;
}

inline void *memset(void *dest, int c, size_t size) {
    asm ("rep stosb" :: "D"(dest), "a"(c), "c"(size) : "memory");

    return dest;
}

inline size_t strlen(const char *str) {
    const char *end = str;
    while (*end) {
        end++;
    }

    return end - str;
}

inline int strcmp(const char *str1, const char *str2) {
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }

    return (*str1) - (*str2);
}

inline int strncmp(const char *str1, const char *str2, size_t n) {
    while (n && *str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
        n--;
    }

    if (!n) {
        return 0;
    }

    return (*str1) - (*str2);
}

inline const char *strstr(const char *haystack, const char *needle) {
    size_t i = 0;

    while (haystack[i] && needle[i]) {
        if (haystack[i] == needle[i]) {
            i++;
        } else {
            haystack++;
            i = 0;
        }
    }

    return (needle[i] == 0) ? haystack : nullptr;
}

}
