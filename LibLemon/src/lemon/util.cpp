#include <lemon/syscall.h>

#include <sys/types.h>
#include <stdint.h>

extern char** environ;

pid_t lemon_spawn(const char* path, int argc, char* const argv[], int flags){
	return syscall(SYS_EXEC, (uintptr_t)path, argc, (uintptr_t)argv, flags, environ);
} 

void lemon_yield(){
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}