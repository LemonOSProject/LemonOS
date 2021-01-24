#include <net/net.h>

#include <net/networkadapter.h>
#include <net/8254x.h>
#include <net/socket.h>

#include <endian.h>
#include <logging.h>
#include <errno.h>

namespace Network {
    NetFS netFS;

    lock_t adaptersLock = 0;
    Vector<NetworkAdapter*> adapters;

    int nextEthernetAdapter = 0;
    int nextLoopbackAdapter = 0;

    void InitializeDrivers(){
	    Intel8254x::DetectAndInitialize();
    }

    void InitializeConnections(){
        if(!mainAdapter) {
            Log::Info("No network adapter found!");
            return;
        }

        Interface::Initialize();
    }

    NetFS* NetFS::instance = nullptr;
    NetFS::NetFS() : Device("net", TypeNetworkStackDevice){
        if(instance){
            return; // Instance already exists!
        }

        instance = this;
        flags = FS_NODE_DIRECTORY;

        DeviceManager::RegisterDevice(*instance);
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
        strcpy(dirent->name, adapter->name);

        dirent->flags = FS_NODE_CHARDEVICE;

        return 1;
    }

    FsNode* NetFS::FindDir(char* name){
        if(strcmp(name, ".") == 0){
            return this;
        } else if(strcmp(name, "..") == 0){
            return DeviceManager::GetDevFS();
        }

        for(NetworkAdapter* adapter : adapters){
            if(strcmp(name, adapter->name) == 0){
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
}