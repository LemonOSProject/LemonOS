#include <net/net.h>

#include <net/networkadapter.h>
#include <net/8254x.h>
#include <endian.h>
#include <logging.h>

namespace Network {
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