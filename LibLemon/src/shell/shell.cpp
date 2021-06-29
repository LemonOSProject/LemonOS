#include <Lemon/Core/Shell.h>

#include <Lemon/Core/URL.h>
#include <Lemon/Services/lemon.shell.h>

namespace Lemon::Shell {
    ShellEndpoint* shellConnection = nullptr;

    ShellEndpoint* GetShell() {
        if(!shellConnection){
            shellConnection = new ShellEndpoint("lemon.shell/Instance");
        }

        return shellConnection;
    }
    
    int Open(const std::string& resource){
        try {
            return GetShell()->Open(resource).status;
        } catch (const EndpointException& e){
            return -1; // Shell probably hasnt started yet
        }
    }

    void ToggleMenu(){
        GetShell()->ToggleMenu();
    }
}
