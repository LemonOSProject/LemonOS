#pragma once

#include <Compiler.h>
#include <stdint.h>

ALWAYS_INLINE static void outportb(uint16_t port, uint8_t value) { asm volatile("outb %1, %0" : : "d"(port), "a"(value)); }

ALWAYS_INLINE static uint8_t inportb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

ALWAYS_INLINE static void outportw(uint16_t port, uint16_t value) { asm volatile("outw %1, %0" : : "d"(port), "a"(value)); }

ALWAYS_INLINE static uint16_t inportw(uint16_t port) {
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

ALWAYS_INLINE static void outportd(uint16_t port, uint32_t value) { asm volatile("outl %1, %0" : : "d"(port), "a"(value)); }

ALWAYS_INLINE static uint32_t inportd(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

ALWAYS_INLINE static void outportl(uint16_t port, uint32_t value) { asm volatile("outl %1, %0" : : "d"(port), "a"(value)); }

ALWAYS_INLINE static uint32_t inportl(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "dN"(port));
    return value;
}
