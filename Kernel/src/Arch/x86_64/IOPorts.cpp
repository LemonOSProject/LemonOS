#include <stdint.h>

void outportb(uint16_t port, uint8_t value) { asm volatile("outb %1, %0" : : "dN"(port), "a"(value)); }

uint8_t inportb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

void outportw(uint16_t port, uint16_t value) { asm volatile("outw %1, %0" : : "dN"(port), "a"(value)); }

uint16_t inportw(uint16_t port) {
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

void outportd(uint16_t port, uint32_t value) { asm volatile("outl %1, %0" : : "dN"(port), "a"(value)); }

uint32_t inportd(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

void outportl(uint16_t port, uint32_t value) { asm volatile("outl %1, %0" : : "dN"(port), "a"(value)); }

uint32_t inportl(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "dN"(port));
    return value;
}
