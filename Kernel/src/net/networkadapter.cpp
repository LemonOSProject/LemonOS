#include <net/networkadapter.h>

#include <list.h>
#include <logging.h>
#include <assert.h>

namespace Network {
    NetworkAdapter* mainAdapter;

    List<NetworkAdapter*> adapters;

    void AddAdapter(NetworkAdapter* a){
        adapters.add_back(a);
    }

    void FindMainAdapter(){
        for(int i = 0; i < adapters.get_length(); i++){
            if(adapters[i]->GetLink() == LinkUp){
                mainAdapter = adapters[i];
                return;
            }
        }
    }

    void NetworkAdapter::SendPacket(void* data, size_t len){
        assert(!"NetworkAdapter: Base class SendPacket has been called");
    }
}