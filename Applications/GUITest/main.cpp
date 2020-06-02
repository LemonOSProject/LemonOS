#include <stdio.h>
#include <assert.h>

#include <gfx/graphics.h>
#include <gfx/surface.h>

#include <gui/window.h>
#include <stdlib.h>

#include <core/msghandler.h>

surface_t fbSurface;

rgba_colour_t backgroundColor = {64, 128, 128};

void OnPaint(surface_t* surface){
    Lemon::Graphics::DrawRect(0, 0, 640, 480, 255, 0, 0, surface);
}

int main(){
    Lemon::GUI::Window* win = new Lemon::GUI::Window("Test Window", {640, 480});
    win->OnPaint = OnPaint;

    for(;;){
        win->Paint();
    }
}