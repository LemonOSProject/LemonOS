#include <net/net.h>

#include <net/networkadapter.h>
#include <net/8254x.h>
#include <net/socket.h>

#include <endian.h>
#include <logging.h>

namespace Network {
    Socket* ports[PORT_MAX + 1];

    void InitializeDrivers(){
	    Intel8254x::DetectAndInitialize();
    }

    void InitializeConnections(){
        if(!mainAdapter) {
            Log::Info("No network adapter found!");
            return;
        }

        Interface::Initialize();
    }

    unsigned short AllocatePort(Socket& sock){
        unsigned short port = EPHEMERAL_PORT_RANGE_START;

        while(port <= EPHEMERAL_PORT_RANGE_END && AcquirePort(sock, port)) port++;

        if(port > EPHEMERAL_PORT_RANGE_END){
            Log::Warning("[Network] Could not allocate ephemeral port!");
            return 0;
        }

        return port;
    }

    int AcquirePort(Socket& sock, unsigned int port){
        if(port > PORT_MAX){
            Log::Warning("[Network] AcquirePort: Invalid port: %d", port);
            return -1;
        }

        if(ports[port]){
            Log::Warning("[Network] AcquirePort: Port %d in use!", port);
            return -2;
        }

        ports[port] = &sock;
        
        return 0;
    }

    void ReleasePort(unsigned short port){
        assert(port <= PORT_MAX);

        ports[port] = nullptr;
    }
}