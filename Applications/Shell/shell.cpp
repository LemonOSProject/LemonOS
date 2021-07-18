#include <Lemon/Core/Shell.h>
#include <Lemon/IPC/Interface.h>
#include <string.h>
#include <stdexcept>

#include "shell.h"

ShellInstance::ShellInstance(const Lemon::Handle& svc, const char* name) : shellSrv(svc, name, 512) {

}

void ShellInstance::OnPeerDisconnect(const Lemon::Handle& client) {
    // Do nothing
}

void ShellInstance::OnOpen(const Lemon::Handle& client, const std::string& url) {
    // TODO: stub
    Shell::OpenResponse response{ ENOSYS };
    Lemon::EndpointQueue(client.get(), Shell::ResponseOpen, sizeof(Shell::OpenResponse), reinterpret_cast<uintptr_t>(&response));
}

void ShellInstance::OnToggleMenu(const Lemon::Handle& client) {
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
    Lemon::Handle client;
    while(shellSrv.Poll(client, m)){
        HandleMessage(client, m);
    }
}
