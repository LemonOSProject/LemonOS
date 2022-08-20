#include <Lemon/Graphics/Graphics.h>
#include <Lemon/Graphics/Surface.h>
#include <Lemon/GUI/Window.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

Surface imageSurf;

void OnPaint(surface_t* surface){
    memset(surface->buffer, 0, surface->width * surface->height * 4);
    surface->Blit(&imageSurf, {0, 0});
}

int main(){
    Lemon::Graphics::LoadImage("/system/lemon/resources/alphatest.png", &imageSurf);

    Lemon::GUI::Window* win = new Lemon::GUI::Window("Test Window", {imageSurf.width, imageSurf.height}, WINDOW_FLAGS_TRANSPARENT, Lemon::GUI::WindowType::Basic);

    win->OnPaint = OnPaint;
    win->Paint();

    while(!win->closed){
		Lemon::WindowServer::Instance()->Poll();

        Lemon::LemonEvent ev;
        while(win->PollEvent(ev)){
            if(ev.event == Lemon::EventWindowClosed){
                delete win;
                return 0;
            }
        }


        win->Paint();

        Lemon::WindowServer::Instance()->Wait();
    }

    delete win;
    return 0;
}