#include <lemon/syscall.h>

void lemon_yield(){
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}