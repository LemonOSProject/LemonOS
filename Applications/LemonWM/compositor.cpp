#include "lemonwm.h"

#include <gui/colours.h>

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
    if((cTime - lastRender) < (16666667 / 2)) return; // Cap at 120 FPS

    lastRender = cTime;

    surface_t* renderSurface = &wm->surface;
    
    if(wm->redrawBackground){
        DrawRect(0, 0, renderSurface->width, renderSurface->height, backgroundColor, renderSurface);
        wm->redrawBackground = false;
    }

    for(WMWindow* win : wm->windows){
        win->Draw(renderSurface);
    }

    if(wm->contextMenuActive){
        DrawRect(wm->contextMenuBounds.x, wm->contextMenuBounds.y, wm->contextMenuBounds.width, wm->contextMenuBounds.height, Lemon::colours[Lemon::Colour::ContentBackground], renderSurface);

        int ypos = wm->contextMenuBounds.y;

        for(ContextMenuItem& item : wm->menu.items){
            if(PointInRect({wm->contextMenuBounds.pos.x, ypos, CONTEXT_ITEM_WIDTH, CONTEXT_ITEM_HEIGHT},wm->input.mouse.pos)){
                DrawRect(wm->contextMenuBounds.x + 24, ypos,  wm->contextMenuBounds.width - 24, CONTEXT_ITEM_HEIGHT, Lemon::colours[Lemon::Colour::Foreground], renderSurface);
            }

            DrawString(item.name.c_str(), wm->contextMenuBounds.x + 28, ypos + 4, 0, 0, 0, renderSurface);
            ypos += CONTEXT_ITEM_HEIGHT;
        }
    }

    surfacecpyTransparent(renderSurface, &mouseCursor, wm->input.mouse.pos);

    if(wm->screenSurface.buffer)
        surfacecpy(&wm->screenSurface, renderSurface);

    if(wm->contextMenuActive)
        DrawRect(wm->contextMenuBounds, backgroundColor, renderSurface);
    
    DrawRect(wm->input.mouse.pos.x, wm->input.mouse.pos.y, mouseCursor.width, mouseCursor.height, backgroundColor, renderSurface);
}