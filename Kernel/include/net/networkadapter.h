#pragma once

#include <device.h>
#include <net/net.h>

enum {
    LinkDown,
    LinkUp,
};

namespace Network{
    class NetworkAdapter : public Device {
    protected:

        int linkState = LinkDown;
        List<NetworkPacket> queue;
    public:
        MACAddress mac;
        virtual void SendPacket(void* data, size_t len);

        virtual int GetLink() { return linkState; }
        virtual int QueueSize() { return queue.get_length(); }
        virtual NetworkPacket Dequeue() { 
            if(queue.get_length()) {
                return queue.remove_at(0); 
            } else {
                return {nullptr, 0};
            }
        }

        virtual ~NetworkAdapter() = default;
    };

    extern NetworkAdapter* mainAdapter;

    void AddAdapter(NetworkAdapter* a);
    void FindMainAdapter();
}