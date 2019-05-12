#define SYSCALL_ISR 0x69

#include <core/syscall.h>

void syscall(uint32_t call, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4){
	__asm__ volatile("int $0x69" :: "a"(call), "b"(arg0), "c"(arg1), "d"(arg2), "S"(arg3), "D"(arg4));
}