#pragma once

#include <device.h>
#include <net/net.h>
#include <scheduler.h>

enum {
    LinkDown,
    LinkUp,
};

namespace Network{
    class NetworkAdapter : public Device {
    protected:
        static int nextDeviceNumber;
    
        int linkState = LinkDown;
        List<NetworkPacket> queue;

        lock_t threadLock = 0;
        Scheduler::GenericThreadBlocker blocker;
    public:
        MACAddress mac;
        
        NetworkAdapter();
        
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
        virtual NetworkPacket DequeueBlocking() {
            Scheduler::BlockCurrentThread(blocker, threadLock);

            if(queue.get_length()){
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