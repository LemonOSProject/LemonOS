#include "lemonwm.h"

using namespace Lemon::Graphics;

rgba_colour_t backgroundColor = {64, 128, 128};

unsigned int operator-(const timespec& t1, const timespec& t2){
    return (t1.tv_sec - t2.tv_sec) * 1000000000 + (t1.tv_nsec - t2.tv_nsec);
}

CompositorInstance::CompositorInstance(WMInstance* wm){
    this->wm = wm;
    clock_gettime(CLOCK_BOOTTIME, &lastRender);
}

void CompositorInstance::Paint(){
    timespec cTime;
    clock_gettime(CLOCK_BOOTTIME, &cTime);
    if((cTime - lastRender) < 16666667) return; // Cap at 60 FPS

    lastRender = cTime;

    surface_t* renderSurface = &wm->surface;
    
    if(wm->redrawBackground){
        DrawRect(0, 0, renderSurface->width, renderSurface->height, backgroundColor, renderSurface);
        wm->redrawBackground = false;
    }

    for(WMWindow* win : wm->windows){
        win->Draw(renderSurface);
    }

    surfacecpyTransparent(renderSurface, &mouseCursor, wm->input.mouse.pos);

    if(wm->screenSurface.buffer)
        surfacecpy(&wm->screenSurface, renderSurface);

    DrawRect(wm->input.mouse.pos.x, wm->input.mouse.pos.y, mouseCursor.width, mouseCursor.height, backgroundColor, renderSurface);
}