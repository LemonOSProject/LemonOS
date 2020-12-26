#include <lemon/core/shell.h>
#include <lemon/ipc/interface.h>
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
    /*while(auto m = shellSrv.Poll()){
        paintTaskbar = true;
        if(m->msg.protocol == 0){ // Disconnected
            continue;
        }

        if(m->msg.protocol == LEMON_MESSAGE_PROTOCOL_SHELLCMD){
            Lemon::Shell::ShellCommand* cmd = (Lemon::Shell::ShellCommand*)m->msg.data;

            switch (cmd->cmd)
            {
            case Lemon::Shell::LemonShellAddWindow:
                {
                    ShellWindow* win = new ShellWindow();

                    char* title = (char*)malloc(cmd->titleLength + 1);
                    strncpy(title, cmd->windowTitle, cmd->titleLength);
                    title[cmd->titleLength] = 0; // Null terminate

                    win->title = title;

                    free(title);

                    win->id = cmd->windowID;
                    win->state = cmd->windowState;

                    windows.insert(std::pair<int, ShellWindow*>(cmd->windowID, win));
                    
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
                ShellWindow* win;
                try{
                    win = windows.at(cmd->windowID);
                } catch (const std::out_of_range& e){
                    printf("[Shell] Warning: LemonShellSetActive: Window ID out of range\n");
                    break;
                }

                if(RemoveWindow) RemoveWindow(win);

                windows.erase(cmd->windowID);
                break;
            } case Lemon::Shell::LemonShellToggleMenu:
                showMenu = !showMenu;
                menu->Minimize(!showMenu);
                break;
            case Lemon::Shell::LemonShellSetWindowState: {
                ShellWindow* win;
                try{
                    win = windows.at(cmd->windowID);
                } catch (const std::out_of_range& e){
                    printf("[Shell] Warning: LemonShellSetActive: Window ID out of range\n");
                    break;
                }

                win->lastState = win->state;
                win->state = cmd->windowState;

                if(win->state == Lemon::Shell::ShellWindowStateActive){
                    if(active && active != win){
                        active->state = Lemon::Shell::ShellWindowStateNormal;
                    }

                    active = win;
                }

                if(RefreshWindows) RefreshWindows();
                break;
            } case Lemon::Shell::LemonShellOpen: {
                char* path = (char*)malloc(cmd->pathLength + 1);

                strncpy(path, cmd->path, cmd->pathLength);
                path[cmd->pathLength] = 0;

                Open(path);

                free(path);
                break;
            } default:
                break;
            }
        }
    }*/
}

void ShellInstance::Open(char* path){
    
}

void ShellInstance::Update(){
    PollCommands();
}
