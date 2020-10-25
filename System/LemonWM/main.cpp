#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <assert.h>

#include <core/msghandler.h>
#include <gui/window.h>
#include <core/fb.h>
#include <core/input.h>
#include <core/cfgparser.h>

#include <time.h>

#ifdef __lemon__
    #include <lemon/spawn.h>
#endif

#include "lemonwm.h"

surface_t fbSurface;
surface_t renderSurface;

extern rgba_colour_t backgroundColor;

int main(){
    CreateFramebufferSurface(fbSurface);
    renderSurface = fbSurface;
    renderSurface.buffer = new uint8_t[fbSurface.width * fbSurface.height * 4];
    
    sockaddr_un srvAddr;
    strcpy(srvAddr.sun_path, Lemon::GUI::wmSocketAddress);
    srvAddr.sun_family = AF_UNIX;
    WMInstance wm = WMInstance(renderSurface, srvAddr);

    Lemon::Graphics::DrawRect(0, 0, renderSurface.width, renderSurface.height, 0, 0, 0, &fbSurface);

	CFGParser cfgParser = CFGParser("/system/lemon/lemonwm.cfg");
    cfgParser.Parse();

    std::string bgPath = "/initrd/bg3.png";

    for(auto item : cfgParser.GetItems()){
        for(auto entry : item.second){
            if(!entry.name.compare("useBackgroundImage")){
                if(!(entry.value.compare("yes") && entry.value.compare("true"))){
                    wm.compositor.useImage = true;
                } else {
                    wm.compositor.useImage = false;
                }
            } else if(!entry.name.compare("backgroundImage")){
                bgPath = entry.value;
            } else if(!entry.name.compare("displayFramerate")){
                if(!(entry.value.compare("yes") && entry.value.compare("true"))){
                    wm.compositor.displayFramerate = true;
                } else {
                    wm.compositor.displayFramerate = false;
                }
            }  else if(!entry.name.compare("capFramerate")){
                if(!(entry.value.compare("yes") && entry.value.compare("true"))){
                    wm.compositor.capFramerate = true;
                } else {
                    wm.compositor.capFramerate = false;
                }
            } 
        }
    }

    if(int e = Lemon::Graphics::LoadImage("/initrd/winbuttons.bmp", &wm.compositor.windowButtons)){
        printf("LemonWM: Warning: Error %d loading buttons.\n", e);
    }

    if(int e = Lemon::Graphics::LoadImage("/initrd/mouse.png", &wm.compositor.mouseCursor)){
        printf("LemonWM: Warning: Error %d loading mouse cursor.\n", e);
    }

    wm.screenSurface = fbSurface;
    wm.compositor.backgroundImage = renderSurface;
    wm.compositor.backgroundImage.buffer = new uint8_t[renderSurface.width * renderSurface.height * 4];
    int bgError = -1;
    if(wm.compositor.useImage && (bgError = Lemon::Graphics::LoadImage(bgPath.c_str(), 0, 0, renderSurface.width, renderSurface.height, &wm.compositor.backgroundImage, true))){
        printf("LemonWM: Warning: Error %d loading background image.\n", bgError);
        wm.compositor.useImage = false;
    }

    for(;;){
        wm.Update();
    }
}
