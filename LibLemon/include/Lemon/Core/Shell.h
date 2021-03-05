#pragma once

#include <Lemon/IPC/Endpoint.h>

namespace Lemon::Shell {
    static const char* shellSocketAddress = "lemonshell";
    
    enum ShellWindowState{
        ShellWindowStateNormal,
        ShellWindowStateActive,
        ShellWindowStateMinimized,
    };

    enum {
        LemonShellToggleMenu = 100,
        LemonShellOpen = 101,
        LemonShellAddWindow = 102,
        LemonShellRemoveWindow = 103,
        LemonShellSetWindowState = 104,
    };

    void AddWindow(long id, short state, const std::string& title, Endpoint& client);
    void RemoveWindow(long id, Endpoint& client);
    void SetWindowState(long id, short state, Endpoint& client);
    
    void Open(const char* path, Endpoint& client);
    void Open(const char* path);

    void ToggleMenu(Endpoint& client);

    void ToggleMenu();
}