#define SYSCALL_ISR 0x69

#include <lemon/syscall.h>

void syscall(uint64_t call, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4){
	asm volatile("int $0x69" :: "a"(call), "b"(arg0), "c"(arg1), "d"(arg2), "S"(arg3), "D"(arg4));
}
