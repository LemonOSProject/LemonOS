#include <lemon/syscall.h>

#include <Lemon/Core/Framebuffer.h>
#include <assert.h>
#include <stdlib.h>

namespace Lemon {
    
long MapFramebuffer(void** ptr, FBInfo& fbInfo) { return syscall(SYS_MAP_FB, ptr, &fbInfo); }

Surface* CreateFramebufferSurface() {
    Surface* surface = (Surface*)malloc(sizeof(Surface));

    CreateFramebufferSurface(*surface);

    return surface;
}

void CreateFramebufferSurface(Surface& surface) {
    FBInfo fbInfo;

    long error = MapFramebuffer(reinterpret_cast<void**>(&surface.buffer), fbInfo);
    assert(!error && surface.buffer);

    surface.width = fbInfo.width;
    surface.height = fbInfo.height;
    surface.depth = fbInfo.bpp;
}
} // namespace Lemon