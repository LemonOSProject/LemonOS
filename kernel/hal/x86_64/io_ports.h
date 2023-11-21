#pragma once

#include <stdint.h>

namespace io {

inline uint8_t in8(uint16_t port) {
    volatile uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void out8(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

inline uint8_t in16(uint16_t port) {
    volatile uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void out16(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" :: "a"(val), "Nd"(port));
}

inline uint32_t in32(uint16_t port) {
    volatile uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void out32(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" :: "a"(val), "Nd"(port));
}

}
