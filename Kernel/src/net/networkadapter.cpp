#include <net/networkadapter.h>

#include <list.h>
#include <logging.h>
#include <assert.h>

namespace Network {
    NetworkAdapter* mainAdapter;
    extern Vector<NetworkAdapter*> adapters;

    void FindMainAdapter(){
        for(unsigned i = 0; i < adapters.get_length(); i++){
            if(adapters[i]->GetLink() == LinkUp){
                mainAdapter = adapters[i];
                return;
            }
        }
    }

    NetworkAdapter::NetworkAdapter(AdapterType aType) : Device(TypeNetworkAdapterDevice), type(aType) {
        flags = FS_NODE_CHARDEVICE;
    }

    void NetworkAdapter::SendPacket(void* data, size_t len){
        assert(!"NetworkAdapter: Base class SendPacket has been called");
    }
}