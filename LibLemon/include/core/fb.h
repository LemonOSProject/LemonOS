#ifndef FB_H
#define FB_H

#include <gfx/surface.h>

#ifdef __lemon__
    #include <lemon/fb.h>
#endif 

surface_t* CreateFramebufferSurface();
void CreateFramebufferSurface(surface_t& surface);

#endif