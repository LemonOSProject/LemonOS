#include <device.h>

#include <list.h>
#include <errno.h>
#include <fs/filesystem.h>
#include <fs/fsvolume.h>
#include <math.h>
#include <timer.h>
#include <string.h>
#include <hash.h>
#include <assert.h>
	
class URandom : public Device {
public:
    URandom(const char* name) : Device(name, DeviceTypeUNIXPseudo) { 
        flags = FS_NODE_CHARDEVICE;
    }

    ssize_t Read(size_t, size_t, uint8_t*);
    ssize_t Write(size_t, size_t, uint8_t*);
};
	
class Null : public Device {
public:
    Null(const char* name) : Device(name, DeviceTypeUNIXPseudo) { 
        flags = FS_NODE_CHARDEVICE;
    }

    ssize_t Read(size_t, size_t, uint8_t*);
    ssize_t Write(size_t, size_t, uint8_t*);
};

int Device::nextUnnamedDeviceNumber = 0;

Device::Device(DeviceType type) : isRootDevice(true), type(type) {
    char nameBuf[20];
    strcpy(nameBuf, "unknown");
    itoa(nextUnnamedDeviceNumber++, nameBuf + strlen(nameBuf), 10);

    name = strdup(nameBuf);

    DeviceManager::RegisterDevice(this);
}

Device::Device(const char* name, DeviceType type) : isRootDevice(true), type(type) {
    this->name = strdup(name);

    DeviceManager::RegisterDevice(this);
}

Device::Device(DeviceType type, Device* parent) : isRootDevice(false), type(type) {
    char nameBuf[20];
    strcpy(nameBuf, "unknown");
    itoa(nextUnnamedDeviceNumber++, nameBuf + strlen(nameBuf), 10);

    name = strdup(nameBuf);

    DeviceManager::RegisterDevice(this);
}

Device::Device(const char* name, DeviceType type, Device* parent) : isRootDevice(false), type(type) {
    this->name = strdup(name);
    this->parent = parent;

    DeviceManager::RegisterDevice(this);
}

Device::~Device(){
    DeviceManager::UnregisterDevice(this);
}

void Device::SetName(const char* name){
    if(this->name){
        kfree(this->name);
    }

    this->name = strdup(name);
}

ssize_t Null::Read(size_t offset, size_t size, uint8_t *buffer){
    memset(buffer, -1, size);

    return size;
}

ssize_t Null::Write(size_t offset, size_t size, uint8_t *buffer){
    return size;
}

ssize_t URandom::Read(size_t offset, size_t size, uint8_t *buffer){
    unsigned int num = (rand() ^ ((Timer::GetSystemUptime() / 2) * Timer::GetFrequency())) + rand();
    
    size_t ogSize = size;

    while(size--){
        *(buffer++) = ((num + rand()) >> (rand() % 32)) & 0xFF;
    }

    return ogSize;
}

ssize_t URandom::Write(size_t offset, size_t size, uint8_t *buffer){
    return size;
}

Null null = Null("null");
URandom urand = URandom("urandom");

namespace DeviceManager{
    int64_t nextDeviceID = 1;
    List<Device*>* rootDevices;
    HashMap<int64_t, Device*>* devices;

    class DevFS : public FsNode{
    private:
        fs::FsVolume vol;

    public:
        DevFS(const char* name){
            flags = FS_NODE_DIRECTORY;

            vol.mountPoint = this;
            vol.mountPointDirent = DirectoryEntry(this, name);

            fs::RegisterVolume(&vol);
        }

        int ReadDir(DirectoryEntry* dirPtr, uint32_t index) final {
            if(index >= rootDevices->get_length() + 2){
                return 0;
            }

            if(index == 0){
                strcpy(dirPtr->name, ".");
                dirPtr->flags = FS_NODE_DIRECTORY;

                return 1;
            } else if(index == 1){
                strcpy(dirPtr->name, "..");
                dirPtr->flags = FS_NODE_DIRECTORY;

                return 1;
            } else {
                strcpy(dirPtr->name, rootDevices->get_at(index - 2)->GetName());
                
                return 1;
            }
        }

        FsNode* FindDir(char* name) final {
            if(!strcmp(name, ".")){
                return this;
            } else if(!strcmp(name, "..")){
                return fs::GetRoot();
            }

            for(auto& dev : *rootDevices){
                if(!strcmp(dev->GetName(), name)){
                    return dev;
                }
            }

            return nullptr; // No such device found
        }
    };

    DevFS* devfs;

    void Initialize(){
        devfs = new DevFS("dev");

        devices = new HashMap<int64_t, Device*>();
        rootDevices = new List<Device*>();
    }

    void RegisterDevice(Device* dev){
        dev->SetID(nextDeviceID++);

        devices->insert(dev->ID(), dev);

        if(dev->IsRootDevice()){
            rootDevices->add_back(dev);
        }
    }

    void UnregisterDevice(Device* dev){
        assert(devices->find(dev->ID()));

        if(dev->IsRootDevice()){
            rootDevices->remove(dev);
        }

        devices->remove(dev->ID());
    }

    FsNode* GetDevFS(){
        return devfs;
    }
}