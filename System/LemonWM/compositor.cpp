#include "lemonwm.h"

#include <lemon/gui/colours.h>

static unsigned int fCount = 0;
static unsigned int avgFrametime = 0;
static unsigned int fRate = 0;

using namespace Lemon::Graphics;

rgba_colour_t backgroundColor = {64, 128, 128, 255};

CompositorInstance::CompositorInstance(WMInstance* wm){
    this->wm = wm;
    clock_gettime(CLOCK_BOOTTIME, &lastRender);
}

void CompositorInstance::RecalculateClipping(){
    clips.clear();

    auto doClipping = [this](const Rect& winRect){
        retry:
        for(auto it = clips.begin(); it != clips.end(); it++){
            WMWindowRect& rect = *it;

            // Check if the rectangles intersect
            if(rect.left() < winRect.right() && rect.right() > rect.left() && rect.right() > winRect.left() && rect.top() < winRect.bottom() && rect.bottom() > winRect.top()){
                clips.erase(it); // Remove the current rectangle
                
                clips.splice(clips.end(), rect.Split(winRect)); // Split it

                goto retry; // Now that it has been split, reiterate through the list
            }
        }
    };

    for(WMWindow* win : wm->windows){
        if(win->minimized){
            continue; // Ignore minimized windows
        }

        WMWindowRect winRect = WMWindowRect(win->GetWindowRect(), win);
        doClipping(winRect);

        clips.push_back(winRect);
    }

    if(wm->contextMenuActive){
        doClipping(wm->contextMenuBounds);
    }
}

void CompositorInstance::Paint(){
    if(displayFramerate){
        timespec cTime;
        clock_gettime(CLOCK_BOOTTIME, &cTime);

        unsigned int renderTime = (cTime - lastRender);

        if(avgFrametime)
            avgFrametime = (avgFrametime + renderTime) / 2;
        else
            avgFrametime = renderTime;

        if(++fCount >= 200){
            if(avgFrametime)
                fRate = 1000000000 / avgFrametime;
            fCount = 0;
            avgFrametime = renderTime;
        }

        lastRender = cTime;
    }

    RecalculateClipping();

    surface_t* renderSurface = &wm->surface;
        
    bool hasRedrawnBackground = wm->redrawBackground;
    if(wm->redrawBackground){
        surfacecpy(renderSurface, &backgroundImage);
    }

    for(auto it = clips.begin(); it != clips.end();){
        WMWindowRect& rect = *it;
        WMWindow* win = rect.win;
        
        if(win->Dirty() || wm->redrawBackground){
            win->SetDirty(0);

            while(it != clips.end() && it->win == win){
                rect = *it;
                win->DrawClip(renderSurface, rect);

                if(!wm->redrawBackground){
                    surfacecpy(&wm->screenSurface, renderSurface, rect.rect.pos, rect);
                }

                it++;
            }
        } else while(it != clips.end() && it->win == win){
            it++;
        }
    }

    if(wm->redrawBackground){
        surfacecpy(&wm->screenSurface, renderSurface);
    }

    if(wm->contextMenuActive){
        rect_t bounds = wm->contextMenuBounds;

        DrawRect(bounds.x, bounds.y, bounds.width, bounds.height, Lemon::colours[Lemon::Colour::Background], renderSurface);

        int ypos = bounds.y;

        for(ContextMenuItem& item : wm->menu.items){
            if(PointInRect({bounds.pos.x, ypos, CONTEXT_ITEM_WIDTH, CONTEXT_ITEM_HEIGHT},wm->input.mouse.pos)){
                DrawRect(bounds.x, ypos,  bounds.width, CONTEXT_ITEM_HEIGHT, Lemon::colours[Lemon::Colour::Foreground], renderSurface);
            }

            DrawString(item.name.c_str(), bounds.x + 24, ypos + 3, Lemon::colours[Lemon::Colour::Text], renderSurface);
            ypos += CONTEXT_ITEM_HEIGHT;
        }

        surfacecpy(&wm->screenSurface, renderSurface, bounds.pos, bounds);
    }

    vector2i_t mousePos = wm->input.mouse.pos;

    surfacecpy(&mouseBuffer, renderSurface, {0, 0}, {mousePos, {mouseBuffer.width, mouseBuffer.height}}); // Save what was under the cursor
    surfacecpyTransparent(&mouseBuffer, &mouseCursor);

    surfacecpy(&wm->screenSurface, renderSurface, lastMousePos, {lastMousePos, {mouseCursor.width, mouseCursor.height}});
    surfacecpy(&wm->screenSurface, &mouseBuffer, mousePos);

    lastMousePos = mousePos;

    if(displayFramerate){
        DrawRect(renderSurface->width - 80, 0, 80, 16, 0, 0 ,0, renderSurface);
        DrawString(std::to_string(fRate).c_str(), renderSurface->width - 78, 2, 255, 255, 255, renderSurface);

        if(!wm->redrawBackground){
            surfacecpy(&wm->screenSurface, renderSurface, {renderSurface->width - 80, 0}, {renderSurface->width - 80, 0, 80, 16});
        }
    }

    if(hasRedrawnBackground){
        wm->redrawBackground = false;
    }
}