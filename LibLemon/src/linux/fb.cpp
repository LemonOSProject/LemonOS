#include <lemon/fb.h>

#include <linux/fb.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <stdlib.h>

surface_t* CreateFramebufferSurface(){
    surface_t* surface = (surface_t*)malloc(sizeof(surface_t));

    CreateFramebufferSurface(*surface);

    return surface;
}

void CreateFramebufferSurface(surface_t& surface){
    int fbFile = open("/dev/fb0", O_RDWR);

    assert(fbFile > 0);

    fb_var_screeninfo sInfo;
    assert(!ioctl(fbFile, FBIOGET_VSCREENINFO, &sInfo));

    unsigned len = sInfo.xres * sInfo.yres * 4;
    surface.buffer = (uint8_t*)mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fbFile, 0);
    assert(surface.buffer);

    surface.x = surface.y = 0;
    surface.width = sInfo.xres;
    surface.height = sInfo.yres;
    surface.depth = sInfo.bits_per_pixel;
}