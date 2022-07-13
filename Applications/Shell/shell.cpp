#include <Lemon/Core/Logger.h>
#include <Lemon/Core/Shell.h>
#include <Lemon/Core/URL.h>
#include <Lemon/IPC/Interface.h>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <string.h>
#include <unistd.h>

#include "shell.h"

std::unordered_map<std::string, std::string> fileAssociations = {
    {"txt", "/system/bin/textedit.lef"},
    {"cfg", "/system/bin/textedit.lef"},
    {"json", "/system/bin/textedit.lef"},
    {"c", "/system/bin/textedit.lef"},
    {"asm", "/system/bin/textedit.lef"},
    {"mp3", "/system/bin/audioplayer.lef"},
    {"wav", "/system/bin/audioplayer.lef"},
    {"flac", "/system/bin/audioplayer.lef"},
    {"png", "/system/bin/imgview.lef"},
    {"bmp", "/system/bin/imgview.lef"},
    {"mkv", "/system/bin/videoplayer.lef"},
};

ShellInstance::ShellInstance(const Lemon::Handle& svc, const char* name) : shellSrv(svc, name, 512) {

}

void ShellInstance::OnPeerDisconnect(const Lemon::Handle& client) {
    // Do nothing
}

void ShellInstance::OnOpen(const Lemon::Handle& client, const std::string& urlString) {
    Lemon::URL url(urlString.c_str());

    Shell::OpenResponse response{ EINVAL };
    if(url.IsValid()) {
        std::string filepath = url.Resource();
        size_t dot = filepath.find_last_of('.');
        
        Lemon::Logger::Debug("Opening {}", filepath);

        char* launchArguments[3];
        if(dot != std::string::npos) {
            auto program = fileAssociations.find(filepath.substr(dot + 1));
            if(program != fileAssociations.end()) {
                launchArguments[0] = strdup(program->second.c_str());
                launchArguments[1] = strdup(filepath.c_str());
                launchArguments[2] = NULL;
            } else if(!filepath.substr(dot + 1).compare("lef")) {
                launchArguments[0] = strdup(filepath.c_str());
                launchArguments[1] = NULL;
            } else {
                launchArguments[0] = NULL;
            }

            if(launchArguments[0]) {
                if(fork() == 0) {
                    execv(launchArguments[0], launchArguments);
                } else {
                    int i = 0;
                    while(launchArguments[i]) {
                        free(launchArguments[i++]);
                    }
                }
            }

        }
    } else {
        Lemon::Logger::Warning("Invalid URL {}", urlString);
    }

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
