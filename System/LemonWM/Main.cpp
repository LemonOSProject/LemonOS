#include <Lemon/Services/lemon.lemonwm.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <assert.h>

#include <Lemon/GUI/Window.h>
#include <Lemon/Core/Framebuffer.h>
#include <Lemon/Core/Input.h>
#include <Lemon/Core/JSON.h>

#include <pthread.h>
#include <time.h>

#include "WM.h"

surface_t fbSurface;
surface_t renderSurface;

std::string bgPath = "/system/lemon/backgrounds/bg6.png";

extern rgba_colour_t backgroundColor;
WMInstance* inst;

void* LoadBackground(void*){
    int bgError = -1;
    if(inst->compositor.useImage && (bgError = Lemon::Graphics::LoadImage(bgPath.c_str(), 0, 0, renderSurface.width, renderSurface.height, &inst->compositor.backgroundImage, true))){
        printf("LemonWM: Warning: Error %d loading background image.\n", bgError);
        inst->compositor.useImage = false;
    }

    inst->redrawBackground = true;

    return nullptr;
}

int main(){
    CreateFramebufferSurface(fbSurface);
    renderSurface = fbSurface;
    renderSurface.buffer = new uint8_t[fbSurface.width * fbSurface.height * 4];

    Lemon::Graphics::DrawRect(0, 0, renderSurface.width, renderSurface.height, 128, 0, 0, &fbSurface);

    handle_t svc = Lemon::CreateService("lemon.lemonwm");
    inst = new WMInstance(renderSurface, svc, "Instance");
    WMInstance& wm = *inst;

    Lemon::Graphics::DrawRect(0, 0, renderSurface.width, renderSurface.height, 255, 0, 0, &fbSurface);

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

        if(auto it = values.find("targetFramerate"); it != values.end()){
            auto& v = it->second;

            if(v.IsNumber()){
                long targetFramerate = v.AsSignedNumber();
                if(targetFramerate <= 0){
                    wm.targetFrameDelay = 0;
                    wm.frameDelayThreshold = -1;
                } else {
                    wm.targetFrameDelay = 1000000 / targetFramerate; // in microseconds
                    wm.frameDelayThreshold = wm.targetFrameDelay - (wm.targetFrameDelay / 2);
                }
            } else {} // TODO: Do something when invalid valaue
        }

        if(auto it = values.find("theme"); it != values.end()){
            auto& v = it->second;

            if(v.IsString()){
                wm.themePath = v.AsString();
            } else {} // TODO: Do something when invalid valaue
        }
    } else {
        printf("[LemonWM] Warning: Error parsing JSON configuration file!\n");
    }

    Lemon::Graphics::DrawRect(0, 0, renderSurface.width, renderSurface.height, 255, 0, 128, &fbSurface);

    if(int e = Lemon::Graphics::LoadImage("/system/lemon/resources/winbuttons.png", &wm.compositor.windowButtons)){
        printf("LemonWM: Warning: Error %d loading buttons.\n", e);
    }

    if(int e = Lemon::Graphics::LoadImage("/system/lemon/resources/mouse.png", &wm.compositor.mouseCursor)){
        printf("LemonWM: Warning: Error %d loading mouse cursor.\n", e);
        wm.compositor.mouseCursor.buffer = new uint8_t[4 * 4 * 4];
        wm.compositor.mouseCursor.width = wm.compositor.mouseCursor.height = 4;
    }
    wm.compositor.mouseBuffer = wm.compositor.mouseCursor;
    wm.compositor.mouseBuffer.buffer = new uint8_t[wm.compositor.mouseCursor.width * wm.compositor.mouseCursor.height * 4];

    wm.compositor.backgroundImage = renderSurface;
    wm.compositor.backgroundImage.buffer = new uint8_t[renderSurface.width * renderSurface.height * 4];
    Lemon::Graphics::DrawString("Loading Background...", fbSurface.width / 2 - Lemon::Graphics::GetTextLength("Loading Background...") / 2, fbSurface.height / 2, 255, 255, 255, &wm.compositor.backgroundImage);

    wm.screenSurface.buffer = fbSurface.buffer;

    pthread_t th;
    pthread_create(&th, nullptr, LoadBackground, nullptr);

    for(;;){
        wm.Update();
    }

    return 0;
}
