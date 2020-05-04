#include <lemon/syscall.h>

#include <stdint.h>
#include <sys/types.h>

extern "C" pid_t lemon_spawn(const char* path, int argc, const char** argv, int flags){
	return syscall(SYS_EXEC, (uintptr_t)path, argc, (uintptr_t)argv, flags, 0);
} 
