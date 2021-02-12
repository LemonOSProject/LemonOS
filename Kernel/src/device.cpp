#include <device.h>

#include <list.h>
#include <errno.h>
#include <fs/filesystem.h>
#include <fs/fsvolume.h>
#include <math.h>
#include <timer.h>
#include <string.h>
#include <vector.h>
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
    if(type == DeviceTypeDevFS){
        return; // No need to register device or set name
    }

    char nameBuf[20];
    strcpy(nameBuf, "unknown");
    itoa(nextUnnamedDeviceNumber++, nameBuf + strlen(nameBuf), 10);

    instanceName = strdup(nameBuf);

    DeviceManager::RegisterDevice(this);
}

Device::Device(const char* name, DeviceType type) : isRootDevice(true), type(type) {
    if(type == DeviceTypeDevFS){
        return; // No need to register device or set name
    }

    instanceName = strdup(name);

    DeviceManager::RegisterDevice(this);
}

Device::Device(DeviceType type, Device* parent) : isRootDevice(false), type(type) {
    if(type == DeviceTypeDevFS){
        return; // No need to register device or set name
    }

    char nameBuf[20];
    strcpy(nameBuf, "unknown");
    itoa(nextUnnamedDeviceNumber++, nameBuf + strlen(nameBuf), 10);

    instanceName = strdup(nameBuf);

    DeviceManager::RegisterDevice(this);
}

Device::Device(const char* name, DeviceType type, Device* parent) : isRootDevice(false), type(type) {
    if(type == DeviceTypeDevFS){
        return; // No need to register device or set name
    }

    this->instanceName = strdup(name);
    this->parent = parent;

    DeviceManager::RegisterDevice(this);
}

Device::~Device(){
    DeviceManager::UnregisterDevice(this);
}

void Device::SetInstanceName(const char* name){
    if(this->instanceName){
        kfree(this->instanceName);
    }

    this->instanceName = strdup(name);
}

void Device::SetDeviceName(const char* name){
    if(this->deviceName){
        kfree(this->deviceName);
    }

    this->deviceName = strdup(name);
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
    Vector<Device*>* devices;

    class DevFS : public Device{
    private:
        fs::FsVolume vol;

    public:
        DevFS(const char* name) : Device(DeviceTypeDevFS){
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
                strcpy(dirPtr->name, rootDevices->get_at(index - 2)->InstanceName());
                
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
                if(!strcmp(dev->InstanceName(), name)){
                    return dev;
                }
            }

            return nullptr; // No such device found
        }
    };

    DevFS* devfs;

    void Initialize(){
        devfs = new DevFS("dev");

        devices = new Vector<Device*>();
        rootDevices = new List<Device*>();
    }

    void RegisterDevice(Device* dev){
        dev->SetID(nextDeviceID++);

        devices->add_back(dev);

        if(dev->IsRootDevice()){
            rootDevices->add_back(dev);
        }
    }

    void UnregisterDevice(Device* dev){
        if(dev->IsRootDevice()){
            rootDevices->remove(dev);
        }

        devices->remove(dev);
    }

    FsNode* GetDevFS(){
        return devfs;
    }

    Device* DeviceFromID(int64_t deviceID){
        for(auto& device : *devices){
            if(device->ID() == deviceID){
                return device;
            }
        }

        return nullptr;
    }

    Device* ResolveDevice(const char* path){
        while(*path == '/'){
            path++; // Eat leading slashes
        }

        if(strncmp(path, "dev/", 4) == 0){ // Ignore dev/ at the start
            path += 4;
        }

        while(*path == '/'){
            path++; // Eat leading slashes
        }

        if(*path){
            return nullptr; // Empty string, only slashes or just /dev/
        }

        char pathBuf[PATH_MAX];
        if(strlen(path) > PATH_MAX){
            return nullptr;
        }

        strcpy(pathBuf, path);

		char* file = strtok(pathBuf,"/");

        Device* currentDev = devfs;
		while(file != NULL){ // Iterate through the directories to find the file
			FsNode* node = fs::FindDir(currentDev,file);
			if(!node) {
				Log::Warning("Device %s not found!", file);
				return nullptr;
			}

			if(((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK)){ // Check for symlinks
				assert(!"Symlinks not supported in DevFS");

                return nullptr;
			}

			if((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
				currentDev = reinterpret_cast<Device*>(node);
				file = strtok(NULL, "/");
				continue;
			}

			if((file = strtok(NULL, "/"))){
				Log::Warning("%s is not a device parent!", file);
				return nullptr;
			}

			if(((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK)){ // Check for symlinks
				assert(!"Symlinks not supported in DevFS");

                return nullptr;
			}

			currentDev = reinterpret_cast<Device*>(node);
			break;
		}
		
		return currentDev;
    }

    int64_t EnumerateDevices(int64_t offset, int64_t count, int64_t* list){
        if(offset >= devices->get_length()){
            return 0;
        }

        if(offset + count >= devices->get_length()){
            count = devices->get_length() - offset;
        }

        memcpy(&devices[offset], list, sizeof(int64_t) * count);

        return count;
    }

    int64_t DeviceCount(){
        return devices->get_length();
    }
}