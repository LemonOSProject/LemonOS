#ifndef FB_H
#define FB_H

#include <Lemon/Graphics/Surface.h>

#ifdef __lemon__
#include <Lemon/System/Framebuffer.h>
#endif

namespace Lemon{
    Surface* CreateFramebufferSurface();
    void CreateFramebufferSurface(Surface& surface);
}

#endif
