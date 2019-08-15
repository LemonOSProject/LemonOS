#include <core/syscall.h>

void exit(){
	syscall(SYS_EXIT,0,0,0,0,0);
	for(;;) asm("hlt");
}