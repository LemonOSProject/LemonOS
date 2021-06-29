#include "WM.h"

#include <Lemon/Core/Shell.h>
#include <Lemon/Core/Keyboard.h>
#include <Lemon/Core/SharedMemory.h>

#include <algorithm>
#include <pthread.h>
#include <unistd.h>

WMInstance::WMInstance(surface_t& surface, handle_t svc, const char* ifName) : server(svc, ifName, LEMONWM_MSG_SIZE){
    this->surface = surface;
    this->screenSurface = surface;
    screenSurface.buffer = nullptr;
}

void* _InitializeShellConnection(void* inst){
    return reinterpret_cast<WMInstance*>(inst)->InitializeShellConnection();
}

void* WMInstance::InitializeShellConnection(){
    shellClient = Lemon::Endpoint("lemon.shell/Instance");
    shellConnected = true;

    return nullptr;
}

WMWindow* WMInstance::FindWindow(int id){
    for(WMWindow* win : windows){
        if(win->clientID == id){
            return win;
        }
    }

    return nullptr;
}

void WMInstance::MinimizeWindow(WMWindow* win, bool state){
    win->Minimize(state);

    if(state == false) { // Showing the window and adding to top
        SetActive(win);

        redrawBackground = true;
    } else { // Hiding the window, so if active set not active
        if(active == win){
            SetActive(nullptr);
        }

        if(lastMousedOver == win){
            Lemon::LemonEvent ev;
            ev.event = Lemon::EventMouseExit;

            PostEvent(ev, win);

            lastMousedOver = nullptr;
        }

        redrawBackground = true;

        //if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL));
            //Lemon::Shell::SetWindowState(win->clientID, Lemon::Shell::ShellWindowStateMinimized, shellClient);
    }
}

void WMInstance::MinimizeWindow(int id, bool state){
    WMWindow* win = FindWindow(id);

    if(!win) {
        printf("[LemonWM] Invalid window ID: %d\n", id);
    }

    MinimizeWindow(win, state);
}

void WMInstance::SetActive(WMWindow* win){
    if(active == win) return;

    if(active && active->flags & WINDOW_FLAGS_ALWAYS_ACTIVE){
        WMWindow* oldActive = active;

        active = win;

        MinimizeWindow(oldActive, true);
    } else if(active){
        //if(shellConnected && !(active->flags & WINDOW_FLAGS_NOSHELL) && !active->minimized){
            //Lemon::Shell::SetWindowState(active->clientID, Lemon::Shell::ShellWindowStateNormal, shellClient);
        //}

        if(active == lastMousedOver){
            Lemon::LemonEvent ev;

            ev.event = Lemon::EventMouseExit;

            PostEvent(ev, active);
        }
    }

    active = win;

    if(win){
        //if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
            //Lemon::Shell::SetWindowState(win->clientID, Lemon::Shell::ShellWindowStateActive, shellClient);
        //}
        
        windows.remove(win);
        windows.push_back(win); // Add to top
    }
}

int64_t WMInstance::CreateWindowBuffer(int width, int height, WindowBuffer** buffer){
    size_t windowBufferSize = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F)) + ((width * height * 4 + 0x1F) & (~0x1F) /* Round up to 32 bytes */) * 2;

    int64_t windowBufferKey = Lemon::CreateSharedMemory(windowBufferSize, SMEM_FLAGS_SHARED);
    if(windowBufferKey <= 0){
        printf("[LemonWM] Warning: Error %ld obtaining shared memory for window\n", -windowBufferKey);
        return windowBufferKey;
    }

    WindowBuffer* windowBufferInfo = (WindowBuffer*)Lemon::MapSharedMemory(windowBufferKey);
    windowBufferInfo->currentBuffer = 0;
    windowBufferInfo->buffer1Offset = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F));
    windowBufferInfo->buffer2Offset = ((sizeof(WindowBuffer) + 0x1F) & (~0x1F)) + ((width * height * 4 + 0x1F) & (~0x1F) /* Round up to 32 bytes*/);

    *buffer = windowBufferInfo;

    return windowBufferKey;
}

void WMInstance::OnPeerDisconnect(handle_t client){
    WMWindow* win = FindWindow(client);

    if(!win){
        return;
    }

    if(active == win){
        SetActive(nullptr);
    }

    if(lastMousedOver == win){
        lastMousedOver = nullptr;
    }
    
    if(menu.owner == win){
        menu.owner = nullptr;
        contextMenuActive = false;
    }

    //if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
        //Lemon::Shell::RemoveWindow(client, shellClient);
    //}
    
    windows.remove(win);
    redrawBackground = true;

    delete win;
}

void WMInstance::OnCreateWindow(handle_t client, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t flags, const std::string& title){
    printf("[LemonWM] Creating Window:    Pos: (%d, %d), Size: %dx%d, Title: %s\n", x,y, width, height, title.c_str());
    
    WindowBuffer* wBufferInfo = nullptr;
    int64_t wBufferKey = CreateWindowBuffer(width, height, &wBufferInfo);
    if(wBufferKey <= 0 || !wBufferInfo){
        printf("[LemonWM] Error obtaining shared memory for window buffer!\n");
        return; // Error obtaining window buffer shared memory
    }

    WMWindow* win = new WMWindow(this, client, client, wBufferKey, wBufferInfo, {x,y}, {width, height}, flags, title);
    windows.push_back(win);

    win->RecalculateButtonRects();
    win->Queue(Lemon::Message(LemonWMServer::ResponseCreateWindow, LemonWMServer::CreateWindowResponse{ .windowID = client, .bufferKey = wBufferKey }));

    //if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
        //Lemon::Shell::AddWindow(client, Lemon::Shell::ShellWindowState::ShellWindowStateNormal, title, shellClient);
    //}
    SetActive(win);

    redrawBackground = true;
}

void WMInstance::OnDestroyWindow(handle_t client){
    WMWindow* win = FindWindow(client);
    if(!win){
        printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
        return;
    }

    if(active == win){
        SetActive(nullptr);
    }

    if(lastMousedOver == win){
        lastMousedOver = nullptr;
    }
    
    if(menu.owner == win){
        menu.owner = nullptr;
        contextMenuActive = false;
    }

    //if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
        //Lemon::Shell::RemoveWindow(client, shellClient);
    //}

    windows.remove(win);
    redrawBackground = true;

    delete win;
}

void WMInstance::OnSetTitle(handle_t client, const std::string& title){
    WMWindow* win = FindWindow(client);
    if(!win){
        printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
        return;
    }

    win->title = title;
}

void WMInstance::OnUpdateFlags(handle_t client, uint32_t flags){
    WMWindow* win = FindWindow(client);
    if(!win){
        printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
        return;
    }

    win->flags = flags;
}

void WMInstance::OnRelocate(handle_t client, int32_t x, int32_t y){
    WMWindow* win = FindWindow(client);
    if(!win){
        printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
        return;
    }

    win->pos = {x, y};
    redrawBackground = true;
}

void WMInstance::OnGetPosition(handle_t client){
    WMWindow* win = FindWindow(client);
    if(!win){
        printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
        return;
    }

    win->Queue(Lemon::Message(LemonWMServer::ResponseGetPosition, LemonWMServer::GetPositionResponse{win->pos.x, win->pos.y}));
}

void WMInstance::OnGetScreenBounds(handle_t client){
    WMWindow* win = FindWindow(client);
    if(!win){
        printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
        return;
    }

    win->Queue(Lemon::Message(LemonWMServer::ResponseGetScreenBounds, LemonWMServer::GetScreenBoundsResponse{surface.width, surface.height}));
}

void WMInstance::OnResize(handle_t client, int32_t width, int32_t height){
    WMWindow* win = FindWindow(client);
    if(!win){
        printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
        return;
    }

    WindowBuffer* wBufferInfo = nullptr;
    int64_t wBufferKey = CreateWindowBuffer(width, height, &wBufferInfo);
    if(wBufferKey <= 0){
        return; // Error obtaining window buffer
    }

    win->Queue(Lemon::Message(LemonWMServer::ResponseResize, ResizeResponse{ .bufferKey = wBufferKey }));
    win->Resize({width, height}, wBufferKey, wBufferInfo);

    redrawBackground = true;
}

void WMInstance::OnMinimize(handle_t client, int64_t windowID, bool minimized){
    MinimizeWindow(windowID, minimized);
}

void WMInstance::OnDisplayContextMenu(handle_t client, int32_t x, int32_t y, const std::string& entries){
    WMWindow* win = FindWindow(client);
    if(!win){
        printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
        return;
    }

    contextMenuBounds = {win->pos + (vector2i_t){x, y}, {CONTEXT_ITEM_WIDTH, 0}};

    if(!(win->flags & WINDOW_FLAGS_NODECORATION)){
        contextMenuBounds.y += WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS;
    }

    menu.items.clear();
    
    int i = 0;
    size_t oldPos = 0;
    size_t pos = 0;
    while((pos = entries.find(";", oldPos)) != std::string::npos){
        size_t cPos = entries.find(",", oldPos);
        if(cPos >= pos || cPos == std::string::npos){
            menu.items.push_back(ContextMenuItem(entries.substr(oldPos, pos), i++, 0));
        } else {
            menu.items.push_back(ContextMenuItem(entries.substr(cPos + 1, pos - (cPos + 1)), i++, std::stoi(entries.substr(oldPos, cPos))));
        }

        oldPos = pos + 1;
        contextMenuBounds.height += CONTEXT_ITEM_HEIGHT;
    }

    menu.owner = win;

    redrawBackground = true;
    contextMenuActive = true;
}

void WMInstance::OnPong(handle_t client){
    WMWindow* win = FindWindow(client);
    if(!win){
        printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
        return;
    }
}

void WMInstance::Poll(){
    Lemon::Message m;
    handle_t client;

    while(server.Poll(client, m)){
        if(client > 0){
            HandleMessage(client, m);
        }
    }
}

void WMInstance::PostEvent(const Lemon::LemonEvent& ev, WMWindow* win){
    win->SendEvent(ev);
}

void WMInstance::MouseDown(){
    if(contextMenuActive){
        if(Lemon::Graphics::PointInRect(contextMenuBounds, input.mouse.pos)){
            return;
        } else {
            contextMenuActive = false;
            redrawBackground = true;
        }
    }

    WMWindow* pressed = nullptr;

    auto it = windows.rbegin();
    while(it != windows.rend()) {
        WMWindow* win = *it++;
        assert(win);

        if(PointInWindow(win, input.mouse.pos)){
            if(win->minimized) continue;

            pressed = win;

            SetActive(win);

            if(PointInWindowProper(win, input.mouse.pos)){
                Lemon::LemonEvent ev;
                ev.event = Lemon::EventMousePressed;
                ev.mousePos = input.mouse.pos - active->pos;

                if(!(win->flags & WINDOW_FLAGS_NODECORATION)) ev.mousePos = ev.mousePos - (vector2i_t){1, 25};

                PostEvent(ev, active);
            } else if(input.mouse.pos.y - win->pos.y < WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS && input.mouse.pos.y - win->pos.y > WINDOW_BORDER_THICKNESS) { // Check if user pressed titlebar
                if(Lemon::Graphics::PointInRect(win->GetCloseRect(), input.mouse.pos)){
                    win->closeBState = ButtonStatePressed;
                    return;
                } else if(Lemon::Graphics::PointInRect(win->GetMinimizeRect(), input.mouse.pos)) {
                    win->minimizeBState = ButtonStatePressed;
                    return;
                }

                drag = true;
                dragOffset = {input.mouse.pos.x - win->pos.x, input.mouse.pos.y - win->pos.y};
            } else { // Resize the window
                resize = true;
                if (Lemon::Graphics::PointInRect(win->GetBottomBorderRect(), input.mouse.pos)){
                    rect_t left = win->GetLeftBorderRect(); // Resize corner if within 10 pixels of border bounds
                    left.width += 10;

                    rect_t right = win->GetRightBorderRect();
                    right.width += 10;
                    right.x -= 10;

                    if (Lemon::Graphics::PointInRect(left, input.mouse.pos)){
                        resizePoint = ResizePoint::BottomLeft;
                    } else if (Lemon::Graphics::PointInRect(right, input.mouse.pos)){
                        resizePoint = ResizePoint::BottomRight;
                    } else {
                        resizePoint = ResizePoint::Bottom;
                    }
                } else if (Lemon::Graphics::PointInRect(win->GetTopBorderRect(), input.mouse.pos)){
                    rect_t left = win->GetLeftBorderRect(); // Resize corner if within 10 pixels of border bounds
                    left.width += 10;

                    rect_t right = win->GetRightBorderRect();
                    right.width += 10;
                    right.x -= 10;

                    if (Lemon::Graphics::PointInRect(left, input.mouse.pos)){
                        resizePoint = ResizePoint::TopLeft;
                    } else if (Lemon::Graphics::PointInRect(right, input.mouse.pos)){
                        resizePoint = ResizePoint::TopRight;
                    } else {
                        resizePoint = ResizePoint::Top;
                    }
                } else if (Lemon::Graphics::PointInRect(win->GetLeftBorderRect(), input.mouse.pos)){
                    // We have already checked for bottom and top so don't bother checking again
                    resizePoint = ResizePoint::Left;
                } else{
                    // We have already checked for bottom and top so don't bother checking again
                    resizePoint = ResizePoint::Right;
                }
                resizeStartPos = input.mouse.pos;
            }

            break;
        }
    };

    if(!pressed){
        SetActive(nullptr); // We must have clicked out of the active window
    }
}

void WMInstance::MouseRight(bool pressed){
    for(auto it = windows.rbegin(); it != windows.rend(); it++){
        WMWindow* win = *it;
            
        if(PointInWindowProper(win, input.mouse.pos)){
            Lemon::LemonEvent ev;

            if(pressed){
                ev.event = Lemon::EventRightMousePressed;
            } else {
                ev.event = Lemon::EventRightMouseReleased;
            }

            ev.mousePos = input.mouse.pos - win->pos;
            if(!(win->flags & WINDOW_FLAGS_NODECORATION)) ev.mousePos = ev.mousePos - (vector2i_t){1, 25};

            PostEvent(ev, win);
        }
    }
}

void WMInstance::MouseUp(){
    if(contextMenuActive && Lemon::Graphics::PointInRect(contextMenuBounds, input.mouse.pos)){
        Lemon::LemonEvent ev;
        ev.event = Lemon::EventWindowCommand;

        ContextMenuItem item = menu.items[(input.mouse.pos.y - contextMenuBounds.y) / CONTEXT_ITEM_HEIGHT];

        ev.windowCmd = item.id;

        PostEvent(ev, menu.owner);

        contextMenuActive = false;

        redrawBackground = true;
        return;
    }

    contextMenuActive = resize = drag = false;

    if(active){
        if(PointInWindowProper(active, input.mouse.pos)){
            Lemon::LemonEvent ev;
            ev.event = Lemon::EventMouseReleased;
            ev.mousePos = input.mouse.pos - active->pos;

            if(!(active->flags & WINDOW_FLAGS_NODECORATION)) ev.mousePos = ev.mousePos - (vector2i_t){1, 25};

            PostEvent(ev, active);
        } else if (Lemon::Graphics::PointInRect(active->GetCloseRect(), input.mouse.pos)){
            Lemon::LemonEvent ev;
            ev.event = Lemon::EventWindowClosed;

            PostEvent(ev, active);
        } else if (Lemon::Graphics::PointInRect(active->GetMinimizeRect(), input.mouse.pos)){
            MinimizeWindow(active, true);
        }
    }
}

void WMInstance::MouseMove(){
    if(drag && active){
        active->pos = input.mouse.pos - dragOffset; // Move window
        if(active->pos.y < 0) active->pos.y = 0;

        redrawBackground = true;
    } else if(resize && active){
        Lemon::LemonEvent ev;
        ev.event = Lemon::EventWindowResize;

        if(resizePoint == ResizePoint::BottomRight) {
            ev.resizeBounds = (input.mouse.pos - active->pos);
            if(!(active->flags & WINDOW_FLAGS_NODECORATION)) ev.resizeBounds.y -= WINDOW_TITLEBAR_HEIGHT - WINDOW_BORDER_THICKNESS;
        } else if (resizePoint == ResizePoint::Bottom) {
            ev.resizeBounds = {active->size.x, input.mouse.pos.y - active->pos.y};
            if(!(active->flags & WINDOW_FLAGS_NODECORATION)) ev.resizeBounds.y -= WINDOW_TITLEBAR_HEIGHT - WINDOW_BORDER_THICKNESS;
        } else if (resizePoint == ResizePoint::Right) {
            ev.resizeBounds = {input.mouse.pos.x - active->pos.x, active->size.y};
        } else {
            return;
        }

        if(ev.resizeBounds.x < 128) ev.resizeBounds.x = 128;
        if(ev.resizeBounds.y < 64) ev.resizeBounds.y = 64;

        PostEvent(ev, active);
        
        resizeStartPos = input.mouse.pos;
    } else {
        bool windowFound = false;
        for(auto it = windows.rbegin(); it != windows.rend(); it++){ 
            WMWindow* win = *it;
            assert(win);

            if(win->minimized){
                continue;
            }

            if(PointInWindowProper(win, input.mouse.pos)){
                windowFound = true;
                Lemon::LemonEvent ev;

                if(win == lastMousedOver){
                    ev.event = Lemon::EventMouseMoved;
                } else {
                    ev.event = Lemon::EventMouseEnter;

                    if(lastMousedOver){
                        Lemon::LemonEvent ev;
                        ev.event = Lemon::EventMouseExit;

                        PostEvent(ev, lastMousedOver);
                    }
                }

                ev.mousePos = input.mouse.pos - win->pos;

                if(!(win->flags & WINDOW_FLAGS_NODECORATION)) ev.mousePos = ev.mousePos - (vector2i_t){WINDOW_BORDER_THICKNESS, WINDOW_TITLEBAR_HEIGHT};
                PostEvent(ev, win);

                lastMousedOver = win; // This window is now the last moused over
                break;
            }
        }

        if(!windowFound){ // Check if the last window moused over is still under the cursor
            if(lastMousedOver){
                Lemon::LemonEvent ev;
                ev.event = Lemon::EventMouseExit;

                PostEvent(ev, lastMousedOver);
            }

            lastMousedOver = nullptr; // Set to null in case the mouse is not above any window
        }
    }
}

void WMInstance::KeyUpdate(int key, bool pressed){
    if((key == KEY_GUI || (key == ' ' && input.keyboard.alt)) && pressed){
        try {
            Lemon::Shell::ToggleMenu();
        } catch(const Lemon::EndpointException& e){
            // Do nothing, shell probably hasnt started yet
        }
    } else if(active && input.keyboard.alt && key == KEY_F4 && pressed) { // Check for Alt + F4
        Lemon::LemonEvent ev;
        ev.event = Lemon::EventWindowClosed;

        PostEvent(ev, active);
    } else if(active){
        Lemon::LemonEvent ev;

        ev.event = pressed ? Lemon::EventKeyPressed : Lemon::EventKeyReleased;
        ev.key = key;
        
        PostEvent(ev, active);
    }
}

void WMInstance::Update(){
    clock_gettime(CLOCK_BOOTTIME, &startTime);

    input.Poll(); // Poll input devices

    Poll(); // Poll for commands

    compositor.Paint(); // Render the frame

    if(targetFrameDelay){
        clock_gettime(CLOCK_BOOTTIME, &endTime);
        long diff = (endTime - startTime) / 1000;
        if(diff < frameDelayThreshold){
            usleep(targetFrameDelay - diff);
        }
    }
}