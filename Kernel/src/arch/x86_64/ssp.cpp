#include <stdint.h>
#include <panic.h>

#define DEFAULT_STACK_GUARD 0xdead69be69efa4db

uintptr_t __stack_chk_guard = DEFAULT_STACK_GUARD;

extern "C"
__attribute__((noreturn))
void __stack_chk_fail(){
    const char* reasons[2] = {"Kernel Stack Overrun", ""};
    KernelPanic(reasons,1);
}