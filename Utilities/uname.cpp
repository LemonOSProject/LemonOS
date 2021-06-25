#include <lemon/syscall.h>

#include <stdio.h>

int main(int argc, char** argv){
    char str[80];
	syscall(SYS_UNAME, (uintptr_t)str,0,0,0,0);

    printf("%s\n", str);
    
    return 0;
}