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
        friend class NetFS;

    public:
        enum AdapterType {
            NetworkAdapterLoopback,
            NetworkAdapterEthernet,
        };

    protected:
        static int nextDeviceNumber;
        unsigned maxCache = 256; // Maximum amount of cached packets, NIC drivers are free to change this
    
        int linkState = LinkDown;

        FastList<NetworkPacket*> cache;
        FastList<NetworkPacket*> queue;

        lock_t cacheLock = 0;
        lock_t queueLock = 0;

        lock_t threadLock = 0;
        Scheduler::GenericThreadBlocker blocker;

        AdapterType type;

    public:
        enum DriverState{
            OK,
            Uninitialized,
            Error,
        };
        DriverState dState = Uninitialized;

        MACAddress mac;
        IPv4Address adapterIP;
        IPv4Address gatewayIP;
        IPv4Address subnetMask;
        
        NetworkAdapter(AdapterType aType);
        
        virtual void SendPacket(void* data, size_t len);

        virtual int GetLink() { return linkState; }
        virtual int QueueSize() { return queue.get_length(); }
        virtual NetworkPacket* Dequeue() { 
            if(queue.get_length()) {
                return queue.remove_at(0); 
            } else {
                return nullptr;
            }
        }
        virtual NetworkPacket* DequeueBlocking() {
            if(!queue.get_length()){
                Scheduler::BlockCurrentThread(blocker, threadLock);
            }

            if(queue.get_length()){
                return queue.remove_at(0); 
            } else {
                return nullptr;
            }
        }
        virtual void CachePacket(NetworkPacket* pkt){
            if(cache.get_length() < maxCache){
                acquireLock(&cacheLock);

                cache.add_back(pkt);

                releaseLock(&cacheLock);
            } else {
                delete pkt;
            }
        };

        virtual ~NetworkAdapter() = default;
    };

    extern NetworkAdapter* mainAdapter;

    void AddAdapter(NetworkAdapter* a);
    void FindMainAdapter();
}