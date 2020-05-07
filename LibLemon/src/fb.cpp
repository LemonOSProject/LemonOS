#include <lemon/syscall.h>

volatile uint8_t* lemon_map_fb(struct FBInfo* fbInfo){
    volatile uint8_t* ptr = 0;
    syscall(SYS_MAP_FB,((uintptr_t)&ptr),(uintptr_t)fbInfo,0,0,0);
    return ptr;
}