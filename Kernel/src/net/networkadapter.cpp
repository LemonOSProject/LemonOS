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

    NetworkAdapter::NetworkAdapter(){
        char buf[16];
        strcpy(buf, "eth");
        itoa(nextDeviceNumber++, buf + 3, 10);

        Device(buf, TypeNetworkAdapterDevice);
    }

    void NetworkAdapter::SendPacket(void* data, size_t len){
        assert(!"NetworkAdapter: Base class SendPacket has been called");
    }
}