#ifndef FB_H
#define FB_H

#include <lemon/graphics/Surface.h>

#ifdef __lemon__
#include <lemon/system/Framebuffer.h>
#endif

namespace Lemon{
    Surface* CreateFramebufferSurface();
    void CreateFramebufferSurface(Surface& surface);
}

#endif
