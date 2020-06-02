#include "lemonwm.h"

using namespace Lemon::Graphics;

rgba_colour_t backgroundColor = {64, 128, 128};

CompositorInstance::CompositorInstance(WMInstance* wm){
    this->wm = wm;
}

void CompositorInstance::Paint(){
    surface_t* renderSurface = &wm->surface;

    DrawRect(0, 0, renderSurface->width, renderSurface->height, backgroundColor, renderSurface);

    for(WMWindow* win : wm->windows){
        win->Draw(renderSurface);
    }

    DrawRect({wm->input.mouse.pos, {5, 5}}, {255, 0, 0}, renderSurface);
}