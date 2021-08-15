#pragma once

#include <stdint.h>
#include <stddef.h>

extern "C" void memcpy_sse2(void* dest, void* src, size_t count);
extern "C" void memcpy_sse2_unaligned(void* dest, void* src, size_t count);
extern "C" void memset32_sse2(void* dest, uint32_t c, uint64_t count);
extern "C" void memset64_sse2(void* dest, uint64_t c, uint64_t count);

inline void memset32_optimized(void* _dest, uint32_t c, size_t count) {
    uint32_t* dest = reinterpret_cast<uint32_t*>(_dest);
    while (count--) {
        *(dest++) = c;
    }
    return;
}

inline void memset64_optimized(void* _dest, uint64_t c, size_t count) {
    uint64_t* dest = reinterpret_cast<uint64_t*>(_dest);
    if (((size_t)dest & 0x7)) {
        while (count--) {
            *(dest++) = c;
        }
        return;
    }

    size_t overflow = (count & 0x7);          // Amount of overflow bytes
    size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

    memset64_sse2(dest, c, size_aligned >> 3);

    while (overflow--) {
        *(dest++) = c;
    }
}

extern "C" void memcpy_optimized(void* dest, void* src, size_t count);