#include <net/networkadapter.h>

#include <list.h>
#include <logging.h>
#include <assert.h>

namespace Network {
    NetworkAdapter* mainAdapter;

    List<NetworkAdapter*> adapters;

    int NetworkAdapter::nextDeviceNumber = 0;

    void AddAdapter(NetworkAdapter* a){
        adapters.add_back(a);
    }

    void FindMainAdapter(){
        for(unsigned i = 0; i < adapters.get_length(); i++){
            if(adapters[i]->GetLink() == LinkUp){
                mainAdapter = adapters[i];
                return;
            }
        }
    }

    NetworkAdapter::NetworkAdapter() : Device(TypeNetworkAdapterDevice) {
        flags = FS_NODE_CHARDEVICE;

        char buf[16];
        strcpy(buf, "eth");
        itoa(nextDeviceNumber++, buf + 3, 10);

        SetName(buf);
    }

    void NetworkAdapter::SendPacket(void* data, size_t len){
        assert(!"NetworkAdapter: Base class SendPacket has been called");
    }
}