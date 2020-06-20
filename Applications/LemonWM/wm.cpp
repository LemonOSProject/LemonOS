#include "lemonwm.h"

#include <gui/window.h>
#include <sys/un.h>
#include <core/shell.h>
#include <core/keyboard.h>

WMInstance::WMInstance(surface_t& surface, sockaddr_un address) : server(address, sizeof(sockaddr_un)){

    this->surface = surface;
    screenSurface.buffer = nullptr;
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

        if(!(win->flags & WINDOW_FLAGS_NOSHELL))
            Lemon::Shell::SetWindowState(win->clientFd, Lemon::Shell::ShellWindowStateMinimized, shellClient);
    }
}

void WMInstance::MinimizeWindow(int id, bool state){
    WMWindow* win = FindWindow(id);

    if(!win) {
        printf("Invalid window ID: %d\n", id);
    }
    if(state)

        printf("minimizing\n");

    MinimizeWindow(win, state);
}

void WMInstance::SetActive(WMWindow* win){
    if(active == win) return;

    if(active && !(active->flags & WINDOW_FLAGS_NOSHELL) && !active->minimized){
        Lemon::Shell::SetWindowState(active->clientFd, Lemon::Shell::ShellWindowStateNormal, shellClient);
    }

    active = win;

    if(win){
        if(!(win->flags & WINDOW_FLAGS_NOSHELL)){
            Lemon::Shell::SetWindowState(win->clientFd, Lemon::Shell::ShellWindowStateActive, shellClient);
        }
        
        windows.remove(win);
        windows.push_back(win); // Add to top
    }
}

void WMInstance::Poll(){
    while(auto m = server.Poll()){
        if(m && m->msg.protocol == LEMON_MESSAGE_PROTCOL_WMCMD){
            auto cmd = (Lemon::GUI::WMCommand*)m->msg.data;

            if(cmd->cmd == Lemon::GUI::WMCreateWindow){
                char* title = (char*)malloc(cmd->create.titleLength + 1);
                strncpy(title, cmd->create.title, cmd->create.titleLength);
                title[cmd->create.titleLength] = 0;

                printf("Creating Window:    Size: %dx%d, Title: %s\n", cmd->create.size.x, cmd->create.size.y, title);

                WMWindow* win = new WMWindow(this, cmd->create.bufferKey);
                win->title = title;
                win->pos = cmd->create.pos;
                win->size = cmd->create.size;
                win->flags = cmd->create.flags;
                win->clientFd = m->clientFd;
                win->RecalculateButtonRects();

                windows.push_back(win);

                if(!(win->flags & WINDOW_FLAGS_NOSHELL)){
                    Lemon::Shell::AddWindow(m->clientFd, Lemon::Shell::ShellWindowState::ShellWindowStateNormal, title, shellClient);
                }
                SetActive(win);
            } else if (cmd->cmd == Lemon::GUI::WMResize){
                WMWindow* win = FindWindow(m->clientFd);

                if(!win){
                    printf("WM: Warning: Unknown Window ID: %d\n", m->clientFd);
                    continue;
                }

                win->Resize(cmd->size, cmd->bufferKey);
            } else if(cmd->cmd == Lemon::GUI::WMDestroyWindow){
                printf("Destroying Window\n");
                WMWindow* win = FindWindow(m->clientFd);

                if(!win){
                    printf("WM: Warning: Unknown Window ID: %d\n", m->clientFd);
                    continue;
                }
                
                Lemon::Shell::RemoveWindow(m->clientFd, shellClient);

                windows.remove(win);
                redrawBackground = true;

                delete win;
            } else if(cmd->cmd == Lemon::GUI::WMMinimize){
                MinimizeWindow(m->clientFd, cmd->minimized);
            } else if(cmd->cmd == Lemon::GUI::WMMinimizeOther){
                MinimizeWindow(cmd->minimizeWindowID, cmd->minimized);
            } else if(cmd->cmd == Lemon::GUI::WMInitializeShellConnection){
                sockaddr_un shellAddr;
                strcpy(shellAddr.sun_path, Lemon::Shell::shellSocketAddress);
                shellAddr.sun_family = AF_UNIX;
                shellClient.Connect(shellAddr, sizeof(sockaddr_un));
                printf("Connected\n");
            }
        } else if (m && m->msg.protocol == 0){ // Client Disconnected
            WMWindow* win = FindWindow(m->clientFd);

            if(!win){
                continue;
            }

            Lemon::Shell::RemoveWindow(m->clientFd, shellClient);
            
            windows.remove(win);
            redrawBackground = true;

            delete win;
        }
    }
}

void WMInstance::PostEvent(Lemon::LemonEvent& ev, WMWindow* win){
    Lemon::LemonMessage* msg = (Lemon::LemonMessage*)malloc(sizeof(Lemon::LemonMessage) + sizeof(Lemon::LemonEvent));
    msg->length = sizeof(Lemon::LemonEvent);
    msg->protocol = LEMON_MESSAGE_PROTCOL_WMEVENT;
    *((Lemon::LemonEvent*)msg->data) = ev;

    server.Send(msg, win->clientFd);
}

void WMInstance::MouseDown(){
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
                printf("resizing\n");
            }

            break;
        }
    } while (it-- != windows.begin());
}

void WMInstance::MouseUp(){
    resize = drag = false;

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
    if(key == KEY_GUI && pressed){
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