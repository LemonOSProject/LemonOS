#include <lemon/syscall.h>

#include <stdint.h>

extern "C" int lemon_spawn(const char* path, int argc, const char** argv, int flags = 0){
	syscall(SYS_EXEC, (uintptr_t)path, argc, (uintptr_t)argv, flags, 0);
} 
