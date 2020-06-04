#pragma once

namespace Lemon::GUI {
    static const char* shellSocketAddress = "lemonshell";
    
    enum {
        LemonShellToggleMenu,
        LemonShellOpen,
        LemonShellAddWindow,
        LemonShellRemoveWindow,
        LemonShellSetActive,
    };

    struct ShellCommand{
        short cmd;
        unsigned short length;
        union
        {
            struct {
            int windowID;
            char windowTitle[];
            };
        };
        
    };
}