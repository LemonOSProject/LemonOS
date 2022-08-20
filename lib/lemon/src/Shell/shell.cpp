#include <lemon/core/Shell.h>

#include <lemon/core/URL.h>
#include <lemon/protocols/lemon.shell.h>

namespace Lemon::Shell {
ShellEndpoint* shellConnection = nullptr;

ShellEndpoint* GetShell() {
    if (!shellConnection) {
        shellConnection = new ShellEndpoint("lemon.shell/Instance");
    }

    return shellConnection;
}

int Open(const std::string& resource) {
    try {
        return GetShell()->Open(resource).status;
    } catch (const EndpointException& e) {
        return -1; // Shell probably hasnt started yet
    }
}

void ToggleMenu() { GetShell()->ToggleMenu(); }
} // namespace Lemon::Shell
