#include <lemon/syscall.h>

#include <stdint.h>
#include <sys/types.h>

extern "C" pid_t lemon_spawn(const char* path, int argc, const char** argv, int flags = 0){
	pid_t pid;

	syscall(SYS_EXEC, (uintptr_t)path, argc, (uintptr_t)argv, flags, &pid);

	return pid;
} 
