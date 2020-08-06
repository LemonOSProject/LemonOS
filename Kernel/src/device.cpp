#include <device.h>

#include <list.h>
#include <errno.h>
#include <fs/filesystem.h>
#include <fs/fsvolume.h>
#include <math.h>
#include <timer.h>
	
class URandom : public Device {
public:
    URandom(const char* name) : Device(name, TypeGenericDevice) { 
        flags = FS_NODE_CHARDEVICE;
    }

    ssize_t Read(size_t, size_t, uint8_t*);
    ssize_t Write(size_t, size_t, uint8_t*);
};
	
class Null : public Device {
public:
    Null(const char* name) : Device(name, TypeGenericDevice) { 
        flags = FS_NODE_CHARDEVICE;
    }

    ssize_t Read(size_t, size_t, uint8_t*);
    ssize_t Write(size_t, size_t, uint8_t*);
};

ssize_t Null::Read(size_t offset, size_t size, uint8_t *buffer){

    memset(buffer, -1, size);
    return size;
}

ssize_t Null::Write(size_t offset, size_t size, uint8_t *buffer){
    return size;
}

ssize_t URandom::Read(size_t offset, size_t size, uint8_t *buffer){
    unsigned int num = rand() ^ ((Timer::GetSystemUptime() / 2) * Timer::GetFrequency()) + rand();
    
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
    List<Device*> devices;

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
            if(index >= devices.get_length() + 2){
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
                strcpy(dirPtr->name, devices[index - 2]->GetName());
                
                return 1;
            }
        }

        FsNode* FindDir(char* name) final {
            if(!strcmp(name, ".")){
                return this;
            } else if(!strcmp(name, "..")){
                return fs::GetRoot();
            }

            for(auto& dev : devices){
                if(!strcmp(dev->GetName(), name)){
                    return dev;
                }
            }

            return nullptr; // No such device found
        }
    };

    DevFS devfs = DevFS("dev");

    void InitializeBasicDevices(){
        RegisterDevice(null);
        RegisterDevice(urand);
    }

    void RegisterDevice(Device& dev){
        devices.add_back(&dev);
    }

    FsNode* GetDevFS(){
        return &devfs;
    }
}