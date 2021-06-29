#include <Lemon/Core/Shell.h>
#include <Lemon/IPC/Interface.h>
#include <string.h>
#include <stdexcept>

#include "shell.h"

ShellInstance::ShellInstance(handle_t svc, const char* name) : shellSrv(svc, name, 512) {

}

void ShellInstance::OnPeerDisconnect(handle_t client) {
    // Do nothing
}

void ShellInstance::OnOpen(handle_t client, const std::string& url) {
    // TODO: stub
    Shell::OpenResponse response{ ENOSYS };
    Lemon::EndpointQueue(client, Shell::ResponseOpen, sizeof(Shell::OpenResponse), reinterpret_cast<uintptr_t>(&response));
}

void ShellInstance::OnToggleMenu(handle_t client) {
    MinimizeMenu(showMenu);
}

void ShellInstance::SetMenu(Lemon::GUI::Window* menu){
    this->menu = menu;
}

void ShellInstance::SetTaskbar(Lemon::GUI::Window* taskbar){
    this->taskbar = taskbar;
}

void ShellInstance::Poll(){
    Lemon::Message m;
    handle_t client;
    while(shellSrv.Poll(client, m)){
        HandleMessage(client, m);
    }
}
