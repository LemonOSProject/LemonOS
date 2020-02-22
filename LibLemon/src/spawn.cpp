#include <lemon/syscall.h>

extern "C" int lemon_spawn(const char* path){
	syscall(SYS_EXEC, (uintptr_t)path, 0, 0, 0, 0);
} 
