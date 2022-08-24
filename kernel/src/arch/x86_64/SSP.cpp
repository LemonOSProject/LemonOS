#include <Panic.h>
#include <stdint.h>

#define DEFAULT_STACK_GUARD 0xdead69be69efa4db

uintptr_t __stack_chk_guard = DEFAULT_STACK_GUARD;

extern "C" __attribute__((noreturn)) void __stack_chk_fail() {
    KernelPanic("Kernel Stack Overrun");
}