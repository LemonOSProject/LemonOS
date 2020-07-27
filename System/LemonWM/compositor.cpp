#include "lemonwm.h"

#include <gui/colours.h>

#ifdef LEMONWM_FRAMERATE_COUNTER
    static unsigned int fCount = 0;
    static unsigned int avgFrametime = 0;
    static unsigned int fRate = 0;
#endif

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

    #ifdef LEMONWM_FRAMERATE_COUNTER
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
    #else
        if((cTime - lastRender) < (11111111 / 2)) return; // Cap at 90 FPS
    #endif

    lastRender = cTime;

    surface_t* renderSurface = &wm->surface;
    
    if(wm->redrawBackground){
        #ifdef LEMONWM_USE_CLIPPING
            auto doClipping = [&](rect_t newRect){
                retry:
                for(WMWindow* win : wm->windows){
                    auto& clips = win->clips;
                    for(auto it = clips.begin(); it != clips.end(); it++){
                        rect_t rect = *it;
                        if(rect.left() < newRect.right() && rect.right() > newRect.left() && rect.top() < newRect.bottom() && rect.bottom() > newRect.top()){
                            clips.erase(it);
                            cclips.erase(it);

                            clips.splice(clips.end(), rect.Split(newRect));
                            cclips.splice(cclips.end(), rect.Split(newRect));
                            goto retry;
                        }
                    }
                }
            };

            for(WMWindow* win : wm->windows){
                win->clips.clear();
            }
            cclips.clear();

            for(WMWindow* win : wm->windows){
                if(win->minimized) continue;

                if(win->flags & WINDOW_FLAGS_NODECORATION) {
                    doClipping({win->pos, win->size});
                    cclips.push_back({win->pos, win->size});
                } else {
                    doClipping({win->pos.x + WINDOW_BORDER_THICKNESS, win->pos.y + WINDOW_BORDER_THICKNESS + WINDOW_TITLEBAR_HEIGHT, win->size.x, win->size.y});
                    cclips.push_back({win->pos.x + WINDOW_BORDER_THICKNESS, win->pos.y + WINDOW_BORDER_THICKNESS + WINDOW_TITLEBAR_HEIGHT, win->size.x, win->size.y});
                }
            }

        #endif

        if(useImage){
            surfacecpy(renderSurface, &backgroundImage);
        } else {
            DrawRect(0, 0, renderSurface->width, renderSurface->height, backgroundColor, renderSurface);
        }

        //wm->redrawBackground = false;
    }

    for(WMWindow* win : wm->windows){
        win->Draw(renderSurface);
    }

    if(wm->contextMenuActive){
        DrawRect(wm->contextMenuBounds.x, wm->contextMenuBounds.y, wm->contextMenuBounds.width, wm->contextMenuBounds.height, Lemon::colours[Lemon::Colour::ContentBackground], renderSurface);
        DrawRectOutline(wm->contextMenuBounds, {0, 0, 0, 255}, renderSurface);

        int ypos = wm->contextMenuBounds.y;

        for(ContextMenuItem& item : wm->menu.items){
            if(PointInRect({wm->contextMenuBounds.pos.x, ypos, CONTEXT_ITEM_WIDTH, CONTEXT_ITEM_HEIGHT},wm->input.mouse.pos)){
                DrawRect(wm->contextMenuBounds.x, ypos,  wm->contextMenuBounds.width, CONTEXT_ITEM_HEIGHT, Lemon::colours[Lemon::Colour::Foreground], renderSurface);
            }

            DrawString(item.name.c_str(), wm->contextMenuBounds.x + 24, ypos + 4, 0, 0, 0, renderSurface);
            ypos += CONTEXT_ITEM_HEIGHT;
        }
    }

    surfacecpyTransparent(renderSurface, &mouseCursor, wm->input.mouse.pos);

    #ifdef LEMONWM_FRAMERATE_COUNTER
    {
        DrawRect(0, 0, 80, 16, 0, 0 ,0, renderSurface);
        DrawString(std::to_string(fRate).c_str(), 2, 2, 255, 255, 255, renderSurface);

        #ifdef LEMONWM_USE_CLIPPING
                surfacecpy(&wm->screenSurface, renderSurface, {0, 0}, {0, 0, 80, 16});
        #endif
    }
    #endif

    
    if(wm->redrawBackground && wm->screenSurface.buffer){
        surfacecpy(&wm->screenSurface, renderSurface);
        wm->redrawBackground = false;
    } else if(wm->screenSurface.buffer){
        #ifdef LEMONWM_USE_CLIPPING
            for(rect_t& r : cclips){
                surfacecpy(&wm->screenSurface, renderSurface, r.pos, r);
            }
        #else
            surfacecpy(&wm->screenSurface, renderSurface);
        #endif
    }

    if(wm->contextMenuActive){
        if(useImage){
            surfacecpy(renderSurface, &backgroundImage, wm->contextMenuBounds.pos, wm->contextMenuBounds);
        } else {
            DrawRect(wm->contextMenuBounds, backgroundColor, renderSurface);
        }
    }
    
    if(useImage){
        surfacecpy(renderSurface, &backgroundImage, wm->input.mouse.pos, {wm->input.mouse.pos, {mouseCursor.width, mouseCursor.height}});
    } else {
        DrawRect(wm->input.mouse.pos.x, wm->input.mouse.pos.y, mouseCursor.width, mouseCursor.height, backgroundColor, renderSurface);
    }

    #ifdef LEMONWM_USE_CLIPPING
        surfacecpy(&wm->screenSurface, renderSurface, wm->input.mouse.pos, {wm->input.mouse.pos, {mouseCursor.width, mouseCursor.height}});
    #else

    #endif
}