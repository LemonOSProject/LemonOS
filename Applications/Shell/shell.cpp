#include <Lemon/Core/Shell.h>
#include <Lemon/IPC/Interface.h>
#include <string.h>
#include <stdexcept>

#include "shell.h"

ShellInstance::ShellInstance(handle_t svc, const char* name) : shellSrv(svc, name, 512) {

}

void ShellInstance::SetMenu(Lemon::GUI::Window* menu){
    this->menu = menu;
}

void ShellInstance::SetTaskbar(Lemon::GUI::Window* taskbar){
    this->taskbar = taskbar;
}

extern bool paintTaskbar;
void ShellInstance::PollCommands(){
    handle_t client;
    Lemon::Message m;
    while(shellSrv.Poll(client, m)){
        paintTaskbar = true;
        if(m.id() == Lemon::MessagePeerDisconnect){ // Disconnected
            continue;
        }

        switch (m.id())
        {
        case Lemon::Shell::LemonShellAddWindow:
            {
                std::string title;
                short state;
                long id;
                if(m.Decode(id, state, title)){
                    continue; // Invalid message
                }

                ShellWindow* win = new ShellWindow();

                win->id = id;
                win->state = state;
                win->title = title;

                windows.insert(std::pair<long, ShellWindow*>(id, win));
                
                if(AddWindow) AddWindow(win);

                if(win->state == Lemon::Shell::ShellWindowStateActive){
                    if(active && active != win){
                        active->state = Lemon::Shell::ShellWindowStateNormal;
                    }

                    active = win;
                }
            }
            break;
        case Lemon::Shell::LemonShellRemoveWindow: {
            long id;
            if(m.Decode(id)){
                continue; // Invalid message
            }

            ShellWindow* win;
            if(auto it = windows.find(id); it != windows.end()){
                win = it->second;
            } else {
                printf("[Shell] Warning: LemonShellRemoveWindow: Window ID out of range\n");
                break;
            }

            if(RemoveWindow) RemoveWindow(win);

            windows.erase(id);
            break;
        } case Lemon::Shell::LemonShellToggleMenu:
            menu->Minimize(!showMenu);
            break;
        case Lemon::Shell::LemonShellSetWindowState: {
            long id;
            short state;
            if(m.Decode(id, state)){
                continue; // Invalid message
            }

            ShellWindow* win;
            if(auto it = windows.find(id); it != windows.end()){
                win = it->second;
            } else {
                printf("[Shell] Warning: LemonShellSetActive: Window ID out of range\n");
                break;
            }

            win->lastState = win->state;
            win->state = state;

            if(win->state == Lemon::Shell::ShellWindowStateActive){
                if(active && active != win){
                    active->state = active->lastState = Lemon::Shell::ShellWindowStateNormal;
                }

                active = win;
            }

            if(RefreshWindows) RefreshWindows();
            break;
        } case Lemon::Shell::LemonShellOpen: {
            break;
        } default:
            break;
        }
    }
}

void ShellInstance::Open(char* path){
    
}

void ShellInstance::Update(){
    PollCommands();
}
