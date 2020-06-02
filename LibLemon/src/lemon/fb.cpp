#include <lemon/syscall.h>

#include <core/fb.h>
#include <assert.h>
#include <stdlib.h>

volatile uint8_t* LemonMapFramebuffer(FBInfo& fbInfo){
    volatile uint8_t* ptr = 0;
    syscall(SYS_MAP_FB,((uintptr_t)&ptr),(uintptr_t)&fbInfo,0,0,0);
    return ptr;
}

surface_t* CreateFramebufferSurface(){
    surface_t* surface = (surface_t*)malloc(sizeof(surface_t));

    CreateFramebufferSurface(*surface);

    return surface;
}

void CreateFramebufferSurface(surface_t& surface){
    FBInfo fbInfo;
    surface.buffer = (uint8_t*)LemonMapFramebuffer(fbInfo);
    assert(surface.buffer);

    surface.x = surface.y = 0;
    surface.width = fbInfo.width;
    surface.height = fbInfo.height;
    surface.depth = fbInfo.bpp;
    surface.pitch = fbInfo.pitch;
}