#pragma once

#include <Device.h>
#include <Net/Net.h>
#include <Scheduler.h>

enum {
    LinkDown,
    LinkUp,
};

class IPSocket;
namespace Network{
    class NetworkAdapter : public Device {
        friend class NetFS;
    public:
        enum AdapterType {
            NetworkAdapterLoopback,
            NetworkAdapterEthernet,
        };

        enum class DriverState{
            OK,
            Uninitialized,
            Error,
        };
        DriverState dState = DriverState::Uninitialized;

        MACAddress mac;

        // All of these are big-endian
        IPv4Address adapterIP = 0; // 0.0.0.0
        IPv4Address gatewayIP = 0; // 0.0.0.0
        IPv4Address subnetMask = 0xFFFFFFFF; // 255.255.255.255 (no subnet)
        int adapterIndex = 0; // Index in the adapters list
        
        NetworkAdapter(AdapterType aType);

        virtual ErrorOr<int> Ioctl(uint64_t cmd, uint64_t arg);
        
        virtual void SendPacket(void* data, size_t len);

        virtual int GetLink() const;
        virtual int QueueSize() const;

        virtual NetworkPacket* Dequeue();
        virtual NetworkPacket* DequeueBlocking();
        virtual void CachePacket(NetworkPacket* pkt);

        void BindToSocket(IPSocket* sock);
        void UnbindSocket(IPSocket* sock);
        void UnbindAllSockets();

        virtual ~NetworkAdapter() = default;

    protected:
        static int nextDeviceNumber;
        unsigned maxCache = 256; // Maximum amount of cached packets, NIC drivers are free to change this
    
        int linkState = LinkDown;

        FastList<NetworkPacket*> cache;
        FastList<NetworkPacket*> queue;

        Semaphore packetSemaphore = Semaphore(0);

        lock_t cacheLock = 0;
        lock_t queueLock = 0;

        lock_t threadLock = 0;

        AdapterType type;

        lock_t boundSocketsLock = 0;
        List<class ::IPSocket*> boundSockets; // If an adapter is destroyed, we need to know what sockets are bound to it
    };

    void AddAdapter(NetworkAdapter* a);
}