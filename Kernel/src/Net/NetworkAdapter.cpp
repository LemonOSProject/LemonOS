#include <Net/Adapter.h>

#include <List.h>
#include <Logging.h>
#include <Assert.h>
#include <Errno.h>
#include <Net/Socket.h>

namespace Network {
    extern Vector<NetworkAdapter*> adapters;

    NetworkAdapter::NetworkAdapter(AdapterType aType) : Device(DeviceTypeNetworkAdapter, NetFS::GetInstance()), type(aType) {
        flags = FS_NODE_CHARDEVICE;
    }

    void NetworkAdapter::SendPacket(void* data, size_t len){
        assert(!"NetworkAdapter: Base class SendPacket has been called");
    }

    int NetworkAdapter::GetLink() const {
        return linkState;
    }

    int NetworkAdapter::QueueSize() const {
        return queue.get_length();
    }

    NetworkPacket* NetworkAdapter::Dequeue() { 
        ScopedSpinLock lock{queueLock};
        if(queue.get_length()) {
            packetSemaphore.SetValue(queue.get_length() - 1);
            return queue.remove_at(0); 
        } else {
            return nullptr;
        }
    }

    NetworkPacket* NetworkAdapter::DequeueBlocking() {
        if(packetSemaphore.Wait()){
            return nullptr; // We were interrupted
        }
        
        ScopedSpinLock lock{queueLock};
        if(queue.get_length()){
            return queue.remove_at(0); 
        } else {
            return nullptr;
        }
    }
    
    void NetworkAdapter::CachePacket(NetworkPacket* pkt){
        if(cache.get_length() < maxCache){
            acquireLock(&cacheLock);

            cache.add_back(pkt);

            releaseLock(&cacheLock);
        } else {
            delete pkt;
        }
    };

    int NetworkAdapter::Ioctl(uint64_t cmd, uint64_t arg){
        Process* currentProcess = Scheduler::GetCurrentProcess();

        if(cmd == SIOCADDRT){
            rtentry* route = reinterpret_cast<rtentry*>(arg);
            if(!Memory::CheckUsermodePointer(arg, sizeof(rtentry), currentProcess->addressSpace)){
                return -EFAULT;
            }

            if(!((route->rt_flags & RTF_GATEWAY) && (route->rt_flags & RTF_UP))){
                return -EINVAL;
            }

            size_t nameLen;
            if(strlenSafe(route->rt_dev, nameLen, currentProcess->addressSpace)){
                return -EFAULT;
            }

            NetworkAdapter* namedAdapter = NetFS::GetInstance()->FindAdapter(route->rt_dev, nameLen);
            if(!namedAdapter){
                return -ENODEV;
            }

            if(currentProcess->euid != 0){
                IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
                    Log::Warning("[Network] NetworkAdapter::Ioctl: Attempted SIOCADDRT as EUID %d!", currentProcess->euid);
                });
                return -EPERM; // We are not root
            }

            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&route->rt_gateway);
            if(addr->sin_family != SocketProtocol::InternetProtocol){
                IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
                    Log::Warning("[Network] NetworkAdapter::Ioctl: Not an IPv4 address!");
                });
                return -EPROTONOSUPPORT; // Not IPv4 address
            }
            namedAdapter->gatewayIP.value = addr->sin_addr.s_addr;

            // Add the gateway to the ARP cache
            MACAddress mac;
            IPLookup(namedAdapter, namedAdapter->gatewayIP, mac);
            return 0;
        } else if(cmd >= SIOCGIFNAME && cmd <= SIOCGIFCOUNT){
            ifreq* req = reinterpret_cast<ifreq*>(arg);

            if(!Memory::CheckUsermodePointer(arg, sizeof(ifreq), currentProcess->addressSpace)){
                return -EFAULT;
            }

            NetworkAdapter* namedAdapter = NetFS::GetInstance()->FindAdapter(req->ifr_name, IF_NAMESIZE);
            if(!namedAdapter && cmd != SIOCGIFNAME /* SIOCGIFNAME is the only call to return something in ifr_name */){
                return -ENODEV;
            }

            switch(cmd){
            case SIOCGIFNAME:
                if(req->ifr_ifindex >= adapters.get_length()){
                    return -ENOENT;
                }
                
                strcpy(req->ifr_name, adapters[req->ifr_ifindex]->instanceName.c_str());
                break;
            case SIOCGIFADDR: {
                sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&req->ifr_addr);
                addr->sin_family = SocketProtocol::InternetProtocol;
                addr->sin_addr.s_addr = namedAdapter->adapterIP.value; // Interface IP adress
                break;
            } case SIOCSIFADDR: {
                if(currentProcess->euid != 0){
                    IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
                        Log::Warning("[Network] NetworkAdapter::Ioctl: Attempted SIOCSIFADDR as EUID %d!", currentProcess->euid);
                    });
                    return -EPERM; // We are not root
                }

                sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&req->ifr_addr);
                if(addr->sin_family != SocketProtocol::InternetProtocol){
                    IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
                        Log::Warning("[Network] NetworkAdapter::Ioctl: Not an IPv4 address!", currentProcess->euid);
                    });
                    return -EPROTONOSUPPORT; // Not IPv4 address
                }

                namedAdapter->adapterIP.value = addr->sin_addr.s_addr;
                break;
            } case SIOCGIFNETMASK: {
                sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&req->ifr_netmask);
                addr->sin_family = SocketProtocol::InternetProtocol;
                addr->sin_addr.s_addr = namedAdapter->subnetMask.value;  // Subnet Mask
                break;
            } case SIOCSIFNETMASK: {
                if(currentProcess->euid != 0){
                    IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
                        Log::Warning("[Network] NetworkAdapter::Ioctl: Attempted SIOCSIFNETMASK as EUID %d!", currentProcess->euid);
                    });
                    return -EPERM; // We are not root
                }

                sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&req->ifr_netmask);
                if(addr->sin_family != SocketProtocol::InternetProtocol){
                    IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
                        Log::Warning("[Network] NetworkAdapter::Ioctl: Not an IPv4 address!", currentProcess->euid);
                    });
                    return -EPROTONOSUPPORT; // Not IPv4 address
                }

                namedAdapter->subnetMask.value = addr->sin_addr.s_addr;
                break;
            } case SIOCGIFHWADDR: {
                memcpy(req->ifr_hwaddr.data, namedAdapter->mac.data, sizeof(MACAddress));  // Hardware (MAC) address
                break;
            } case SIOCGIFINDEX:
                req->ifr_ifindex = namedAdapter->adapterIndex; // Interface index
                break;
            default:
                return -EINVAL;
            }
            return 0;
        } else {
            return -EINVAL;
        }
    }

    void NetworkAdapter::BindToSocket(IPSocket* sock){
        acquireLock(&boundSocketsLock);

        boundSockets.add_back(sock);
        sock->adapter = this;

        releaseLock(&boundSocketsLock);
    }

    void NetworkAdapter::UnbindSocket(IPSocket* sock){
        acquireLock(&boundSocketsLock);

        sock->adapter = nullptr;
        boundSockets.remove(sock);

        releaseLock(&boundSocketsLock);
    }

    void NetworkAdapter::UnbindAllSockets(){
        acquireLock(&boundSocketsLock);

        while(boundSockets.get_length()){
            IPSocket* sock = boundSockets.remove_at(0);
            sock->adapter = nullptr;
        }

        releaseLock(&boundSocketsLock);
    }
}