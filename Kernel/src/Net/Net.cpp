#include <Net/Net.h>

#include <Net/Adapter.h>
#include <Net/Socket.h>

#include <Endian.h>
#include <Logging.h>
#include <Errno.h>
#include <Hash.h>

namespace Network {
    NetFS netFS;

    lock_t adaptersLock = 0;
    Vector<NetworkAdapter*> adapters;

    HashMap<uint32_t, MACAddress> addressCache;

    void InitializeConnections(){
        InitializeNetworkThread();
    }

    int IPLookup(NetworkAdapter* adapter, const IPv4Address& ip, MACAddress& mac){
        if(addressCache.get(ip.value, mac)){
            return 0;
        }

        uint8_t buffer[sizeof(EthernetFrame) + sizeof(ARPHeader)];
        
		EthernetFrame* ethFrame = reinterpret_cast<EthernetFrame*>(buffer);
		ethFrame->etherType = EtherTypeARP;
		ethFrame->src = adapter->mac;
		ethFrame->dest = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        ARPHeader* arp = reinterpret_cast<ARPHeader*>(buffer + sizeof(EthernetFrame));
        arp->destHwAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        arp->srcHwAddr = adapter->mac;
        arp->hLength = 6;
        arp->hwType = 1; // Ethernet

        arp->destPrAddr = ip.value;
        arp->srcPrAddr = adapter->adapterIP.value;
        arp->pLength = 4;
        arp->prType = EtherTypeARP;

        arp->opcode = ARPHeader::ARPRequest;

        Network::Send(buffer, sizeof(EthernetFrame) + sizeof(ARPHeader), adapter);

        assert(CheckInterrupts());

        auto t1 = Timer::GetSystemUptimeStruct();
        int status = 0;
        while(!(status = addressCache.find(ip.value)) && Timer::TimeDifference(Timer::GetSystemUptimeStruct(), t1) < 2000000){
            Scheduler::Yield();
        }

        if(status <= 0){
            IF_DEBUG((debugLevelNetwork >= DebugLevelNormal), {
                Log::Warning("[Network] [ARP] The system is bored waiting for ARP reply.");
            });
            return -EADDRNOTAVAIL;
        }

        status = addressCache.get(ip.value, mac);
        assert(status > 0);

        return 0;
    }

    int Route(const IPv4Address& local, const IPv4Address& dest, MACAddress& mac, NetworkAdapter*& adapter){
        bool isLocalDestination = false;
        IPv4Address localDestination;

        Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] Route: Attempting to find route from %s to %s", to_string(local).c_str(), to_string(dest).c_str());

        if(adapter){
            if(local.value != INADDR_ANY && local.value != adapter->adapterIP.value){ // If the socket address is not 0 then make sure it aligns with the adapter
                return -ENETUNREACH;
            }

            // Check if the destination is within the subnet
            if((dest.value & adapter->subnetMask.value) == (adapter->adapterIP.value & adapter->subnetMask.value)){
                isLocalDestination = true;
                localDestination = dest;
            } else {
                localDestination = adapter->gatewayIP; // Destination is to WAN, 
            }
        } else {
            for(NetworkAdapter* a : adapters){
                if(local.value != INADDR_ANY && a->adapterIP.value != local.value){
                    continue; // Local address does not correspond to the adapter IP address
                }

                // Check if the destination is within the subnet
                if((dest.value & a->subnetMask.value) == (a->adapterIP.value & a->subnetMask.value)){
                    isLocalDestination = true;
                    adapter = a;
                    localDestination = dest; // Destination is already local
                } else if(!isLocalDestination && a->gatewayIP.value > 0){
                    adapter = a; // If a local route has not been found and the adapter has been assigned a gateway, pick this adapter
                    localDestination = a->gatewayIP;
                }
            }
        }

        if(!adapter){
            Log::Warning("[Network] Could not find any adapters! find one!!!!");
            return -ENETUNREACH;
        }

        if(isLocalDestination){
            int status = IPLookup(adapter, dest, mac);
            if(status < 0){
                return status; // Error obtaining MAC address for IP
            }
        } else {
            int status = IPLookup(adapter, adapter->gatewayIP, mac);
            if(status < 0){
                return status; // Error obtaining MAC address for IP
            }
        }
        
        return 0;
    }

    NetFS* NetFS::instance = nullptr;
    NetFS::NetFS() : Device("net", DeviceTypeNetworkStack){
        if(instance){
            return; // Instance already exists!
        }

        instance = this;
        flags = FS_NODE_DIRECTORY;
    }

    int NetFS::ReadDir(DirectoryEntry* dirent, uint32_t index){
        if(index == 0){
            strcpy(dirent->name, ".");

            dirent->flags = FS_NODE_DIRECTORY;
            return 1;
        } else if(index == 1){
            strcpy(dirent->name, "..");

            dirent->flags = FS_NODE_DIRECTORY;
            return 1;
        }

        if(index >= adapters.get_length() + 2){
            return 0; // Out of range
        }

        NetworkAdapter* adapter = adapters[index - 2];
        strcpy(dirent->name, adapter->InstanceName().c_str());

        dirent->flags = FS_NODE_CHARDEVICE;

        return 1;
    }

    FsNode* NetFS::FindDir(const char* name){
        if(strcmp(name, ".") == 0){
            return this;
        } else if(strcmp(name, "..") == 0){
            return DeviceManager::GetDevFS();
        }

        for(NetworkAdapter* adapter : adapters){
            if(strcmp(name, adapter->InstanceName().c_str()) == 0){
                return adapter;
            }
        }

        return nullptr;
    }

    void NetFS::RegisterAdapter(NetworkAdapter* adapter){
        acquireLock(&adaptersLock);

        adapter->adapterIndex = adapters.get_length();
        adapters.add_back(adapter);

        releaseLock(&adaptersLock);
    }

    void NetFS::RemoveAdapter(NetworkAdapter* adapter){
        acquireLock(&adaptersLock);

        for(unsigned i = 0; i < adapters.get_length(); i++){
            if(adapters[i] == adapter){
                for(IPSocket* sock : adapter->boundSockets){
                    sock->adapter = nullptr;
                }
                adapter->boundSockets.clear();

                adapters.erase(i);
                break;
            }
        }

        releaseLock(&adaptersLock);
    }

    NetworkAdapter* NetFS::FindAdapter(const char* name, size_t len){
        for(NetworkAdapter* adapter : adapters){
            if(strncmp(name, adapter->InstanceName().c_str(), len) == 0){
                return adapter;
            }
        }

        return nullptr;
    }

    NetworkAdapter* NetFS::FindAdapter(uint32_t ip){
        for(NetworkAdapter* adapter : adapters){
            if(adapter->adapterIP.value == ip){
                return adapter; // Found adapter with IP address
            }
        }

        return nullptr;
    }
}