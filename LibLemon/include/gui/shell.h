#pragma once

namespace Lemon::GUI {
    static const char* shellSocketAddress = "lemonshell";
    
    enum {
        LemonShellToggleMenu,
        LemonShellOpen,
    };

    struct ShellCommand{
        short cmd;
        unsigned short length;
    };
}