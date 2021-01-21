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
}