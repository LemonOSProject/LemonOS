#ifndef FB_H
#define FB_H

#include <Lemon/Graphics/Surface.h>

#ifdef __lemon__
    #include <Lemon/System/Framebuffer.h>
#endif 

surface_t* CreateFramebufferSurface();
void CreateFramebufferSurface(surface_t& surface);

#endif