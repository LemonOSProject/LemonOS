#include <lemon/syscall.h>

#ifdef __cplusplus
extern "C"{
#endif

uint8_t* lemon_map_fb(struct FBInfo* fbInfo){
    uint8_t* ptr = 0;
    syscall(SYS_MAP_FB,&ptr,fbInfo,0,0,0);
    return ptr;
}

#ifdef __cplusplus
}
#endif