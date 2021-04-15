#include <lemon/syscall.h>

#include <Lemon/Core/Framebuffer.h>
#include <assert.h>
#include <stdlib.h>

long LemonMapFramebuffer(void** ptr, FBInfo& fbInfo){
    return syscall(SYS_MAP_FB, ptr, &fbInfo);
}

surface_t* CreateFramebufferSurface(){
    surface_t* surface = (surface_t*)malloc(sizeof(surface_t));

    CreateFramebufferSurface(*surface);

    return surface;
}

void CreateFramebufferSurface(surface_t& surface){
    FBInfo fbInfo;
    
    long error = LemonMapFramebuffer(reinterpret_cast<void**>(&surface.buffer), fbInfo);
    assert(!error && surface.buffer);

    surface.width = fbInfo.width;
    surface.height = fbInfo.height;
    surface.depth = fbInfo.bpp;
}