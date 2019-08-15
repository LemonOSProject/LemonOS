#include <core/syscall.h>
#include <core/lemon.h>

char* lemon_uname(char* string){
	syscall(SYS_UNAME, string, 0, 0, 0, 0);
	return string;
}

void lemon_sysinfo(lemon_sys_info_t* sys){
	syscall(SYS_GET_SYSINFO, (uint32_t)sys, 0, 0, 0, 0);
}