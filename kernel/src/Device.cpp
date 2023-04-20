#include <Device.h>

#include <Assert.h>
#include <Errno.h>
#include <Fs/Filesystem.h>
#include <Fs/FsVolume.h>
#include <Fs/VolumeManager.h>
#include <Scheduler.h>
#include <List.h>
#include <Math.h>
#include <String.h>
#include <Timer.h>
#include <Vector.h>

class URandom : public Device {
public:
    URandom(const char* name) : Device(name, DeviceTypeUNIXPseudo) {
        type = FileType::CharDevice;

        SetDeviceName("UNIX Random Device");
    }

    ErrorOr<ssize_t> read(size_t, size_t, UIOBuffer*);
    ErrorOr<ssize_t> write(size_t, size_t, UIOBuffer*);
};

class Null : public Device {
public:
    Null(const char* name) : Device(name, DeviceTypeUNIXPseudo) {
        type = FileType::CharDevice;

        SetDeviceName("UNIX Null Device");
    }

    ErrorOr<ssize_t> read(size_t, size_t, UIOBuffer*);
    ErrorOr<ssize_t> write(size_t, size_t, UIOBuffer*);
};

class TTY : public Device {
public:
    TTY(const char* name) : Device(name, DeviceTypeUNIXPseudo) {
        type = FileType::CharDevice;

        SetDeviceName("Process Terminal");
    }

    ErrorOr<File*> Open(size_t flags) override;
};

int Device::nextUnnamedDeviceNumber = 0;

Device::Device(DeviceType type) : isRootDevice(true), m_deviceType(type) {
    if (type == DeviceTypeDevFS) {
        return; // No need to register device or set name
    }

    char nameBuf[20];
    strcpy(nameBuf, "unknown");
    itoa(nextUnnamedDeviceNumber++, nameBuf + strlen(nameBuf), 10);

    instanceName = strdup(nameBuf);

    DeviceManager::RegisterDevice(this);
}

Device::Device(const char* name, DeviceType type) : isRootDevice(true), m_deviceType(type) {
    if (type == DeviceTypeDevFS) {
        return; // No need to register device or set name
    }

    instanceName = strdup(name);

    DeviceManager::RegisterDevice(this);
}

Device::Device(DeviceType type, Device* parent) : isRootDevice(false), m_deviceType(type) {
    if (type == DeviceTypeDevFS) {
        return; // No need to register device or set name
    }

    char nameBuf[20];
    strcpy(nameBuf, "unknown");
    itoa(nextUnnamedDeviceNumber++, nameBuf + strlen(nameBuf), 10);

    instanceName = strdup(nameBuf);

    DeviceManager::RegisterDevice(this);
}

Device::Device(const char* name, DeviceType type, Device* parent) : isRootDevice(false), m_deviceType(type) {
    if (type == DeviceTypeDevFS) {
        return; // No need to register device or set name
    }

    this->instanceName = strdup(name);
    this->parent = parent;

    DeviceManager::RegisterDevice(this);
}

Device::~Device() { DeviceManager::UnregisterDevice(this); }

void Device::SetInstanceName(const char* name) {
    this->instanceName = name;
}

void Device::SetDeviceName(const char* name) {
    this->deviceName = name;
}

ErrorOr<ssize_t> Null::read(size_t offset, size_t size, UIOBuffer* buffer) {
    if(buffer->fill(-1, size)) {
        return EFAULT;
    }

    return size;
}

ErrorOr<ssize_t> Null::write(size_t offset, size_t size, UIOBuffer* buffer) { return size; }

ErrorOr<ssize_t> URandom::read(size_t offset, size_t size, UIOBuffer* buffer) {
    unsigned int num = (rand() ^ ((Timer::GetSystemUptime() / 2) * Timer::GetFrequency())) + rand();

    size_t ogSize = size;

    uint8_t data[4096];
    while (size > 0) {
        for(int i = 0; i < 4096 && i < size; i++) {
            data[i] = ((num + rand()) >> (rand() % 32)) & 0xFF;
        }

        int bytes = 4096;
        if(bytes > size) {
            bytes = size;
        }

        if(buffer->write(data, bytes)) {
            return EFAULT;
        }

        size -= bytes;
    }

    return ogSize;
}

ErrorOr<ssize_t> URandom::write(size_t offset, size_t size, UIOBuffer* buffer) { return size; }

ErrorOr<File*> TTY::Open(size_t flags){
    /*auto stdout = TRY_OR_ERROR(Process::current()->get_handle_as<File>(1));

    assert(stdout->node);
    return stdout->node->Open(flags);*/

    return ENOSYS;
}

Null null("null");
URandom urand("urandom");
TTY tty("tty");

namespace DeviceManager {
int64_t nextDeviceID = 1;
List<Device*>* rootDevices;
Vector<Device*>* devices;

class DevFS : public Device {
public:
    fs::FsVolume vol;

    DevFS(const char* name) : Device(DeviceTypeDevFS) {
        type = FileType::Directory;

        vol.mountPoint = this;
        vol.mountPointDirent = DirectoryEntry(this, name);
    }

    ErrorOr<int> read_dir(DirectoryEntry* dirPtr, uint32_t index) final {
        if (index >= rootDevices->get_length() + 2) {
            return 0;
        }

        if (index == 0) {
            strcpy(dirPtr->name, ".");
            dirPtr->flags = DT_DIR;

            return 1;
        } else if (index == 1) {
            strcpy(dirPtr->name, "..");
            dirPtr->flags = DT_DIR;

            return 1;
        } else {
            strcpy(dirPtr->name, rootDevices->get_at(index - 2)->InstanceName().c_str());

            return 1;
        }
    }

    ErrorOr<FsNode*> find_dir(const char* name) final {
        if (!strcmp(name, ".")) {
            return this;
        } else if (!strcmp(name, "..")) {
            return fs::root();
        }

        for (auto& dev : *rootDevices) {
            if (!strcmp(dev->InstanceName().c_str(), name)) {
                return dev;
            }
        }

        return Error{ENOENT}; // No such device found
    }
};

DevFS* devfs;

void Initialize() {
    devfs = new DevFS("dev");

    devices = new Vector<Device*>();
    rootDevices = new List<Device*>();
}

void RegisterFSVolume(){
    fs::VolumeManager::RegisterVolume(&devfs->vol);
}

void RegisterDevice(Device* dev) {
    dev->SetID(nextDeviceID++);

    devices->add_back(dev);

    if (dev->IsRootDevice()) {
        rootDevices->add_back(dev);
    }
}

void UnregisterDevice(Device* dev) {
    if (dev->IsRootDevice()) {
        rootDevices->remove(dev);
    }

    devices->remove(dev);
}

FsNode* GetDevFS() { return devfs; }

ErrorOr<Device*> DeviceFromID(int64_t deviceID) {
    for (auto& device : *devices) {
        if (device->ID() == deviceID) {
            return device;
        }
    }

    return Error{ENOENT};
}

ErrorOr<Device*> ResolveDevice(const char* path) {
    while (*path == '/') {
        path++; // Eat leading slashes
    }

    if (strncmp(path, "dev/", 4) == 0) { // Ignore dev/ at the start
        path += 4;
    }

    while (*path == '/') {
        path++; // Eat leading slashes
    }

    if (*path) {
        return nullptr; // Empty string, only slashes or just /dev/
    }

    char pathBuf[PATH_MAX];
    if (strlen(path) > PATH_MAX) {
        return nullptr;
    }

    strcpy(pathBuf, path);

    char* strtokSavePtr;
    char* file = strtok_r(pathBuf, "/", &strtokSavePtr);

    Device* currentDev = devfs;
    while (file != NULL) { // Iterate through the directories to find the file
        FsNode* node = TRY_OR_ERROR(fs::find_dir(currentDev, file));
        if (!node) {
            Log::Warning("Device %s not found!", file);
            return nullptr;
        }

        if (node->is_symlink()) { // Check for symlinks
            assert(!"Symlinks not supported in DevFS");

            return nullptr;
        }

        if (node->is_directory()) {
            currentDev = reinterpret_cast<Device*>(node);
            file = strtok_r(NULL, "/", &strtokSavePtr);
            continue;
        }

        if ((file = strtok_r(NULL, "/", &strtokSavePtr))) {
            Log::Warning("%s is not a device parent!", file);
            return nullptr;
        }

        if (node->is_symlink()) { // Check for symlinks
            assert(!"Symlinks not supported in DevFS");

            return nullptr;
        }

        currentDev = reinterpret_cast<Device*>(node);
        break;
    }

    return currentDev;
}

int64_t EnumerateDevices(int64_t offset, int64_t count, int64_t* list) {
    if (offset >= devices->get_length()) {
        return 0;
    }

    if (offset + count >= devices->get_length()) {
        count = devices->get_length() - offset;
    }

    for (unsigned i = offset; i - offset < count && i < devices->get_length(); i++) {
        list[i - offset] = devices->get_at(i)->ID();
    }

    return count;
}

int64_t DeviceCount() { return devices->get_length(); }
} // namespace DeviceManager