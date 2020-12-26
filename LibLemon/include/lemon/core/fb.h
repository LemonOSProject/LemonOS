#ifndef FB_H
#define FB_H

#include <lemon/gfx/surface.h>

#ifdef __lemon__
    #include <lemon/system/fb.h>
#endif 

surface_t* CreateFramebufferSurface();
void CreateFramebufferSurface(surface_t& surface);

#endif