#include <lemon/syscall.h>
#include <lemon/lemon.h>

char* lemon_uname(char* string){
	syscall(SYS_UNAME, string, 0, 0, 0, 0);
	return string;
}
/*
void lemon_sysinfo(lemon_sys_info_t* sys){
	syscall(SYS_GET_SYSINFO, (uintptr_t)sys, 0, 0, 0, 0);
}
*/