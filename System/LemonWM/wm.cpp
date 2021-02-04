#include "lemonwm.h"

#include <lemon/gui/window.h>
#include <lemon/core/shell.h>
#include <lemon/core/keyboard.h>
#include <algorithm>
#include <pthread.h>
#include <lemon/core/sharedmem.h>
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
    redrawBackground = true;

    win->Minimize(state);

    if(state == false) { // Showing the window and adding to top
        SetActive(win);
    } else { // Hiding the window, so if active set not active
        if(active == win){
            SetActive(nullptr);
        }

        if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL))
            Lemon::Shell::SetWindowState(win->clientID, Lemon::Shell::ShellWindowStateMinimized, shellClient);
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

    if(shellConnected && active && !(active->flags & WINDOW_FLAGS_NOSHELL) && !active->minimized){
        Lemon::Shell::SetWindowState(active->clientID, Lemon::Shell::ShellWindowStateNormal, shellClient);
    }

    active = win;

    if(win){
        if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
            Lemon::Shell::SetWindowState(win->clientID, Lemon::Shell::ShellWindowStateActive, shellClient);
        }
        
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

void WMInstance::Poll(){
    Lemon::Message m;
    handle_t client;

    while(server.Poll(client, m)){
        if(client > 0){
            auto cmd = m.id();

            if(cmd == Lemon::MessagePeerDisconnect){
                WMWindow* win = FindWindow(client);

                if(!win){
                    continue;
                }

                if(active == win){
                    SetActive(nullptr);
                }

                if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
                    Lemon::Shell::RemoveWindow(client, shellClient);
                }
                
                windows.remove(win);
                redrawBackground = true;

                delete win;
            } else if(cmd == Lemon::GUI::WMCreateWindow){
                int x = 0;
                int y = 0;
                int width = 0;
                int height = 0;
                unsigned int flags;
                std::string title;

                if(m.Decode(x, y, width, height, flags, title)){
                    printf("[LemonWM] WMCreateWindow: Invalid msg\n");
                    continue; // Invalid or corrupted message, just ignore
                }

                printf("[LemonWM] Creating Window:    Pos: (%d, %d), Size: %dx%d, Title: %s\n", x,y, width, height, title.c_str());
                
                WindowBuffer* wBufferInfo = nullptr;
                int64_t wBufferKey = CreateWindowBuffer(width, height, &wBufferInfo);
                if(wBufferKey <= 0 || !wBufferInfo){
                    continue; // Error obtaining window buffer shared memory
                }

                WMWindow* win = new WMWindow(this, client, client, wBufferKey, wBufferInfo, {x,y}, {width, height}, flags, title);
                win->RecalculateButtonRects();

                windows.push_back(win);

                if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
                    Lemon::Shell::AddWindow(client, Lemon::Shell::ShellWindowState::ShellWindowStateNormal, title, shellClient);
                }
                SetActive(win);

                redrawBackground = true;
            } else if (cmd == Lemon::GUI::WMResizeWindow){
                WMWindow* win = FindWindow(client);

                if(!win){
                    printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
                    continue;
                }

                int width;
                int height;
                if(m.Decode(width, height)){
                    continue; // Invalid or corrupted message
                }

                WindowBuffer* wBufferInfo = nullptr;
                int64_t wBufferKey = CreateWindowBuffer(width, height, &wBufferInfo);
                if(wBufferKey <= 0){
                    continue; // Error obtaining window buffer
                }

                win->Resize({width, height}, wBufferKey, wBufferInfo);

                redrawBackground = true;
            } else if(cmd == Lemon::GUI::WMDestroyWindow){
                printf("Destroying Window\n");
                WMWindow* win = FindWindow(client);

                if(!win){
                    printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
                    continue;
                }

                if(active == win){
                    SetActive(nullptr);
                }
                
                if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
                    Lemon::Shell::RemoveWindow(client, shellClient);
                }

                windows.remove(win);
                redrawBackground = true;

                delete win;
            } else if(cmd == Lemon::GUI::WMSetWindowTitle){
                WMWindow* win = FindWindow(client);

                if(!win){
                    printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
                    continue;
                }
                
                std::string title;
                if(m.Decode(title)){
                    continue; // Invalid or corrupted message
                }

                win->title = title;
            } else if(cmd == Lemon::GUI::WMMinimizeWindow){
                bool minimized = true;
                if(m.Decode(minimized)){
                    continue; // Invalid or corrupted message
                }

                MinimizeWindow(client, minimized);
            } else if(cmd == Lemon::GUI::WMMinimizeOtherWindow){
                bool minimized = true;
                handle_t winID = client;
                if(m.Decode(winID, minimized)){
                    continue; // Invalid or corrupted message
                }

                MinimizeWindow(winID, minimized);
            } else if(cmd == Lemon::GUI::WMInitializeShellConnection){
                pthread_t p;
                pthread_create(&p, nullptr, reinterpret_cast<void*(*)(void*)>(&_InitializeShellConnection), this);
            } else if(cmd == Lemon::GUI::WMDisplayContextMenu){
                WMWindow* win = FindWindow(client);

                if(!win){
                    printf("[LemonWM] Warning: Unknown Window ID: %ld\n", client);
                    continue;
                }

                menu.items.clear();

                vector2i_t menuPos;
                std::string entries;
                if(m.Decode(menuPos.x, menuPos.y, entries)){
                    continue;
                }

                contextMenuBounds = {win->pos + menuPos, {CONTEXT_ITEM_WIDTH, 0}};

                if(!(win->flags & WINDOW_FLAGS_NODECORATION)){
                    contextMenuBounds.y += WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS;
                }
                
                int i = 0;
                size_t pos;
                while((pos = entries.find(";")) != std::string::npos){
                    size_t cPos = entries.find(",");
                    if(cPos >= pos || cPos == std::string::npos){
                        menu.items.push_back(ContextMenuItem(entries.substr(0, pos), i++, 0));
                    } else {
                        menu.items.push_back(ContextMenuItem(entries.substr(cPos + 1, pos - (cPos + 1)), i++, std::stoi(entries.substr(0, cPos))));
                    }

                    entries.erase(0, pos + 1);
                    contextMenuBounds.height += CONTEXT_ITEM_HEIGHT;
                }

                menu.owner = win;

                redrawBackground = true;
                contextMenuActive = true;
            }
        }
    }
}

void WMInstance::PostEvent(Lemon::LemonEvent& ev, WMWindow* win){
    win->PostEvent(ev);
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

    auto it = windows.end();
    do {
        WMWindow* win = *it;

        if(PointInWindow(win, input.mouse.pos)){
            if(win->minimized) continue;

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
    } while (it-- != windows.begin());
}

void WMInstance::MouseRight(bool pressed){
    auto it = windows.end();
    do {
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
    } while (it-- != windows.begin());
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
    if(resize && active){
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
        if(lastMousedOver && !PointInWindowProper(lastMousedOver, input.mouse.pos)){ // Check if the last window moused over is still under the cursor
            Lemon::LemonEvent ev;
            ev.event = Lemon::EventMouseExit;

            PostEvent(ev, lastMousedOver);

            lastMousedOver = nullptr; // Set to null in case the mouse is not above any window
        }

        for(WMWindow* win : windows){ 
            if(PointInWindowProper(win, input.mouse.pos)){
                Lemon::LemonEvent ev;

                if(win == lastMousedOver){
                    ev.event = Lemon::EventMouseMoved;
                } else {
                    ev.event = Lemon::EventMouseEnter;
                }

                ev.mousePos = input.mouse.pos - active->pos;

                if(!(active->flags & WINDOW_FLAGS_NODECORATION)) ev.mousePos = ev.mousePos - (vector2i_t){1, 25};

                PostEvent(ev, win);

                lastMousedOver = win; // This window is now the last moused over

                break;
            }
        }
    }
}

void WMInstance::KeyUpdate(int key, bool pressed){
    if(shellConnected && key == KEY_GUI && pressed){
        Lemon::Shell::ToggleMenu(shellClient);
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

    if(drag && active){
        active->pos = input.mouse.pos - dragOffset; // Move window
        if(active->pos.y < 0) active->pos.y = 0;

        redrawBackground = true;
    }

    compositor.Paint(); // Render the frame

    if(targetFrameDelay){
        clock_gettime(CLOCK_BOOTTIME, &endTime);
        long diff = (endTime - startTime) / 1000;
        if(diff > 0 && diff < frameDelayThreshold){
            usleep(targetFrameDelay - diff);
        }
    }
}