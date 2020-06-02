#include "lemonwm.h"

#include <gui/window.h>
#include <sys/un.h>

WMInstance::WMInstance(surface_t& surface, sockaddr_un address) : server(address, sizeof(sockaddr_un)){
    this->surface = surface;
}

void WMInstance::Poll(){
    while(auto m = server.Poll()){
        if(m && m->msg.protocol == LEMON_MESSAGE_PROTCOL_WMCMD){
            printf("Recieved WM Command\n");

            auto cmd = (Lemon::GUI::WMCommand*)m->msg.data;

            if(cmd->cmd == Lemon::GUI::WMCreateWindow){
                char* title = (char*)malloc(cmd->create.titleLength + 1);
                strncpy(title, cmd->create.title, cmd->create.titleLength);
                title[cmd->create.titleLength] = 0;

                printf("Creating Window:    Size: %dx%d, Title: %s\n", cmd->create.size.x, cmd->create.size.y, title);

                WMWindow* win = new WMWindow(cmd->create.bufferKey);
                win->title = title;
                win->pos = cmd->create.pos;
                win->size = cmd->create.size;
                win->flags = cmd->create.flags;
                win->clientFd = m->clientFd;

                windows.push_back(win);
            }
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
    for(WMWindow* win : windows){
        if(PointInWindow(win, input.mouse.pos)){
            windows.remove(win);
            windows.push_back(win); // Add to top

            active = win;

            if(PointInWindowProper(win, input.mouse.pos)){
                Lemon::LemonEvent ev;
                ev.event = Lemon::EventMousePressed;
                ev.mousePos = input.mouse.pos - active->pos;
                PostEvent(ev, active);
            } else if(input.mouse.pos.y - win->pos.y < 25) { // Check if user pressed titlebar
                drag = true;
                dragOffset = {input.mouse.pos.x - win->pos.x, input.mouse.pos.y - win->pos.y};
            }
        }
    }
}

void WMInstance::MouseUp(){
    drag = false;

    if(active && PointInWindowProper(active, input.mouse.pos)){
        Lemon::LemonEvent ev;
        ev.event = Lemon::EventMouseReleased;
        ev.mousePos = input.mouse.pos - active->pos;
        PostEvent(ev, active);
    }
}

void WMInstance::KeyUpdate(int key, bool pressed){
    if(active){
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
    }
    
    compositor.Paint(); // Render the frame
}