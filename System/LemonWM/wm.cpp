#include "lemonwm.h"

#include <gui/window.h>
#include <sys/un.h>
#include <core/shell.h>
#include <core/keyboard.h>
#include <algorithm>
#include <pthread.h>

WMInstance::WMInstance(surface_t& surface, sockaddr_un address) : server(address, sizeof(sockaddr_un)){

    this->surface = surface;
    screenSurface.buffer = nullptr;
}

void* WMInstance::InitializeShellConnection(){
    sockaddr_un shellAddr;
    strcpy(shellAddr.sun_path, Lemon::Shell::shellSocketAddress);
    shellAddr.sun_family = AF_UNIX;
    shellClient.Connect(shellAddr, sizeof(sockaddr_un));

    shellConnected = true;

    return nullptr;
}

WMWindow* WMInstance::FindWindow(int id){
    for(WMWindow* win : windows){
        if(win->clientFd == id){
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
            Lemon::Shell::SetWindowState(win->clientFd, Lemon::Shell::ShellWindowStateMinimized, shellClient);
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
        Lemon::Shell::SetWindowState(active->clientFd, Lemon::Shell::ShellWindowStateNormal, shellClient);
    }

    active = win;

    if(win){
        if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
            Lemon::Shell::SetWindowState(win->clientFd, Lemon::Shell::ShellWindowStateActive, shellClient);
        }
        
        windows.remove(win);
        windows.push_back(win); // Add to top
    }
}

void WMInstance::Poll(){
    while(auto m = server.Poll()){
        if(m && m->msg.protocol == LEMON_MESSAGE_PROTOCOL_WMCMD){
            auto cmd = (Lemon::GUI::WMCommand*)m->msg.data;

            if(cmd->cmd == Lemon::GUI::WMCreateWindow){
                char* title = (char*)malloc(cmd->create.titleLength + 1);
                strncpy(title, cmd->create.title, cmd->create.titleLength);
                title[cmd->create.titleLength] = 0;

                printf("[LemonWM] Creating Window:    Size: %dx%d, Title: %s\n", cmd->create.size.x, cmd->create.size.y, title);

                WMWindow* win = new WMWindow(this, cmd->create.bufferKey);
                win->title = title;
                win->pos = cmd->create.pos;
                win->size = cmd->create.size;
                win->flags = cmd->create.flags;
                win->clientFd = m->clientFd;
                win->RecalculateButtonRects();

                windows.push_back(win);

                if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
                    Lemon::Shell::AddWindow(m->clientFd, Lemon::Shell::ShellWindowState::ShellWindowStateNormal, title, shellClient);
                }
                SetActive(win);

                redrawBackground = true;
            } else if (cmd->cmd == Lemon::GUI::WMResize){
                WMWindow* win = FindWindow(m->clientFd);

                if(!win){
                    printf("[LemonWM] Warning: Unknown Window ID: %d\n", m->clientFd);
                    continue;
                }

                win->Resize(cmd->size, cmd->bufferKey);
            } else if(cmd->cmd == Lemon::GUI::WMDestroyWindow){
                printf("Destroying Window\n");
                WMWindow* win = FindWindow(m->clientFd);

                if(!win){
                    printf("[LemonWM] Warning: Unknown Window ID: %d\n", m->clientFd);
                    continue;
                }

                if(active == win){
                    SetActive(nullptr);
                }
                
                if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
                    Lemon::Shell::RemoveWindow(m->clientFd, shellClient);
                }

                windows.remove(win);
                redrawBackground = true;

                delete win;
            } else if(cmd->cmd == Lemon::GUI::WMSetTitle){
                WMWindow* win = FindWindow(m->clientFd);

                if(!win){
                    printf("[LemonWM] Warning: Unknown Window ID: %d\n", m->clientFd);
                    continue;
                }
                
                char* title = (char*)malloc(cmd->titleLength + 1);
                strncpy(title, cmd->title, cmd->titleLength);
                title[cmd->titleLength] = 0;

                if(win->title) free(win->title);
                win->title = title;
            } else if(cmd->cmd == Lemon::GUI::WMMinimize){
                MinimizeWindow(m->clientFd, cmd->minimized);
            } else if(cmd->cmd == Lemon::GUI::WMMinimizeOther){
                MinimizeWindow(cmd->minimizeWindowID, cmd->minimized);
            } else if(cmd->cmd == Lemon::GUI::WMInitializeShellConnection){
                pthread_t p;
                pthread_create(&p, nullptr, reinterpret_cast<void*(*)(void*)>(&WMInstance::InitializeShellConnection), this);
            } else if(cmd->cmd == Lemon::GUI::WMOpenContextMenu){
                WMWindow* win = FindWindow(m->clientFd);

                if(!win){
                    printf("[LemonWM] Warning: Unknown Window ID: %d\n", m->clientFd);
                    continue;
                }

                menu.items.clear();

                char buf[256];
                contextMenuBounds = {win->pos + cmd->contextMenuPosition, {CONTEXT_ITEM_WIDTH, 0}};

                if(!(win->flags & WINDOW_FLAGS_NODECORATION)){
                    contextMenuBounds.y += WINDOW_TITLEBAR_HEIGHT + WINDOW_BORDER_THICKNESS;
                }
                
                Lemon::GUI::WMContextMenuEntry* item = cmd->contextMenu.contextEntries;
                for(unsigned int i = 0; i < cmd->contextMenu.contextEntryCount; i++){

                    if(((uintptr_t)item) + sizeof(Lemon::GUI::WMContextMenuEntry) + item->length - (uintptr_t)(m->msg.data) > m->msg.length){
                        printf("[LemonWM] Invalid context menu item length: %d", item->length);
                        continue;
                    }

                    strncpy(buf, item->data, std::min<int>(static_cast<int>(item->length), 255));
                    buf[std::min<int>(static_cast<int>(item->length), 255)] = 0;

                    menu.items.push_back(ContextMenuItem(buf, i, item->id));

                    contextMenuBounds.height += CONTEXT_ITEM_HEIGHT;
                    
                    item = reinterpret_cast<Lemon::GUI::WMContextMenuEntry*>(reinterpret_cast<uintptr_t>(item) + item->length + sizeof(Lemon::GUI::WMContextMenuEntry));
                }

                menu.owner = win;
                contextMenuActive = true;
            }
        } else if (m && m->msg.protocol == 0){ // Client Disconnected
            WMWindow* win = FindWindow(m->clientFd);

            if(!win){
                continue;
            }

            if(active == win){
                SetActive(nullptr);
            }

            if(shellConnected && !(win->flags & WINDOW_FLAGS_NOSHELL)){
                Lemon::Shell::RemoveWindow(m->clientFd, shellClient);
            }
            
            windows.remove(win);
            redrawBackground = true;

            delete win;
        }
    }
}

void WMInstance::PostEvent(Lemon::LemonEvent& ev, WMWindow* win){
    server.Send(Lemon::Message(LEMON_MESSAGE_PROTOCOL_WMEVENT, ev), win->clientFd);
}

void WMInstance::MouseDown(){
    if(Lemon::Graphics::PointInRect(contextMenuBounds, input.mouse.pos)){
        return;
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
                    if (Lemon::Graphics::PointInRect(win->GetLeftBorderRect(), input.mouse.pos)){
                        resizePoint = ResizePoint::BottomLeft;
                    } else if (Lemon::Graphics::PointInRect(win->GetRightBorderRect(), input.mouse.pos)){
                        resizePoint = ResizePoint::BottomRight;
                    } else {
                        resizePoint = ResizePoint::Bottom;
                    }
                } else if (Lemon::Graphics::PointInRect(win->GetTopBorderRect(), input.mouse.pos)){
                    if (Lemon::Graphics::PointInRect(win->GetLeftBorderRect(), input.mouse.pos)){
                        resizePoint = ResizePoint::TopLeft;
                    } else if (Lemon::Graphics::PointInRect(win->GetRightBorderRect(), input.mouse.pos)){
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

        printf("[LemonWM] Context Item: %s, ID: %d\n", item.name.c_str(), item.id);
        PostEvent(ev, menu.owner);

        contextMenuActive = false;
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
            ev.resizeBounds = active->size + (input.mouse.pos - resizeStartPos);
        } else if (resizePoint == ResizePoint::Bottom) {
            ev.resizeBounds = active->size + (vector2i_t){0, (input.mouse.pos.y - resizeStartPos.y)};
        } else if (resizePoint == ResizePoint::Right) {
            ev.resizeBounds = active->size + (vector2i_t){(input.mouse.pos.x - resizeStartPos.x), 0};
        } else {
            return;
        }

        if(ev.resizeBounds.x < 100) ev.resizeBounds.x = 100;
        if(ev.resizeBounds.y < 50) ev.resizeBounds.y = 50;

        PostEvent(ev, active);
        
        resizeStartPos = input.mouse.pos;

        redrawBackground = true;
    } else if (active && PointInWindowProper(active, input.mouse.pos)){
        Lemon::LemonEvent ev;
        ev.event = Lemon::EventMouseMoved;
        ev.mousePos = input.mouse.pos - active->pos;

        if(!(active->flags & WINDOW_FLAGS_NODECORATION)) ev.mousePos = ev.mousePos - (vector2i_t){1, 25};

        PostEvent(ev, active);
    }
}

void WMInstance::KeyUpdate(int key, bool pressed){
    if(shellConnected && key == KEY_GUI && pressed){
        Lemon::Shell::ToggleMenu(shellClient);
    } else if(active){
        Lemon::LemonEvent ev;

        ev.event = pressed ? Lemon::EventKeyPressed : Lemon::EventKeyReleased;
        ev.key = key;
        
        PostEvent(ev, active);
    }
}

void WMInstance::Update(){
    Poll(); // Poll for commands
    
    input.Poll(); // Poll input devices

    if(drag && active){
        active->pos = input.mouse.pos - dragOffset; // Move window
        if(active->pos.y < 0) active->pos.y = 0;

        redrawBackground = true;
    }

    compositor.Paint(); // Render the frame
}