#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <assert.h>

#include <lemon/gui/window.h>
#include <lemon/core/fb.h>
#include <lemon/core/input.h>
#include <lemon/core/json.h>

#include <time.h>

#include "lemonwm.h"

surface_t fbSurface;
surface_t renderSurface;

extern rgba_colour_t backgroundColor;

int main(){
    CreateFramebufferSurface(fbSurface);
    renderSurface = fbSurface;
    renderSurface.buffer = new uint8_t[fbSurface.width * fbSurface.height * 4];

    Lemon::Graphics::DrawRect(0, 0, renderSurface.width, renderSurface.height, 128, 0, 0, &fbSurface);

    handle_t svc = Lemon::CreateService("lemon.lemonwm");
    WMInstance wm = WMInstance(renderSurface, svc, "Instance");

    Lemon::Graphics::DrawRect(0, 0, renderSurface.width, renderSurface.height, 255, 0, 0, &fbSurface);

    std::string bgPath = "/initrd/bg3.png";

	Lemon::JSONParser configParser("/system/lemon/lemonwm.json");
    auto json = configParser.Parse();
    if(json.IsObject()){
        std::map<Lemon::JSONKey, Lemon::JSONValue>& values = *json.object;

        if(auto it = values.find("useBackgroundImage"); it != values.end()){
            auto& v = it->second;

            if(v.IsBool()){
                wm.compositor.useImage = v.AsBool();
            } else {} // TODO: Do something when invalid valaue
        }

        if(auto it = values.find("backgroundImage"); it != values.end()){
            auto& v = it->second;

            if(v.IsString()){
                bgPath = v.AsString();
            } else {} // TODO: Do something when invalid valaue
        }

        if(auto it = values.find("displayFramerate"); it != values.end()){
            auto& v = it->second;

            if(v.IsBool()){
                wm.compositor.displayFramerate = v.AsBool();
            } else {} // TODO: Do something when invalid valaue
        }

        if(auto it = values.find("capFramerate"); it != values.end()){
            auto& v = it->second;

            if(v.IsBool()){
                wm.compositor.capFramerate = v.AsBool();
            } else {} // TODO: Do something when invalid valaue
        }
    } else {
        printf("[LemonWM] Warning: Error parsing JSON configuration file!\n");
    }

    Lemon::Graphics::DrawRect(0, 0, renderSurface.width, renderSurface.height, 255, 0, 128, &fbSurface);

    if(int e = Lemon::Graphics::LoadImage("/initrd/winbuttons.png", &wm.compositor.windowButtons)){
        printf("LemonWM: Warning: Error %d loading buttons.\n", e);
    }

    if(int e = Lemon::Graphics::LoadImage("/initrd/mouse.png", &wm.compositor.mouseCursor)){
        printf("LemonWM: Warning: Error %d loading mouse cursor.\n", e);
        wm.compositor.mouseCursor.buffer = new uint8_t[4 * 4];
        wm.compositor.mouseCursor.width = wm.compositor.mouseCursor.height = 4;
    }

    wm.compositor.backgroundImage = renderSurface;
    wm.compositor.backgroundImage.buffer = new uint8_t[renderSurface.width * renderSurface.height * 4];
    int bgError = -1;
    if(wm.compositor.useImage && (bgError = Lemon::Graphics::LoadImage(bgPath.c_str(), 0, 0, renderSurface.width, renderSurface.height, &wm.compositor.backgroundImage, true))){
        printf("LemonWM: Warning: Error %d loading background image.\n", bgError);
        wm.compositor.useImage = false;
    }

    wm.screenSurface.buffer = fbSurface.buffer;

    for(;;){
        wm.Update();
    }
}
