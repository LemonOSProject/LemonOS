#include <Fs/VolumeManager.h>

#include <Fs/FsVolume.h>
#include <Panic.h>

namespace fs {
namespace VolumeManager {

FsVolume* systemVolume = nullptr;
List<FsVolume*>* volumes;

int nextVolumeID = 1;
int nextVolumeName = 0;

lock_t volumeManagerLock = 0;

void Initialize() { volumes = new List<FsVolume*>(); }

FsVolume* FindVolume(const char* name) {
    for (auto volume : *volumes) {
        if (!strcmp(volume->mountPointDirent.name, name)) {
            return volume;
        }
    }

    return nullptr;
}

void MountSystemVolume() {
    FsNode* devFS = fs::ResolvePath("/dev");
    assert(devFS);

    DirectoryEntry ent;
    int i = 0;
    while (fs::ReadDir(devFS, &ent, i++)) {
        FsDriver* drv = nullptr;
        FsNode* device = nullptr;
        if ((device = fs::FindDir(devFS, ent.name)) && device->IsCharDevice()) {
            if ((drv = fs::IdentifyFilesystem(device))) {
                int ret = Mount(device, drv, "system");
                if (!ret) {
                    return;
                }
            }
        }
    }
}

// Identify filesystem and mount device using optional name
int Mount(FsNode* device, const char* name) {
    if (!device->IsCharDevice()) {
        Log::Error("Fs::VolumeManager::Mount: Not a device!");
        return VolumeErrorNotDevice;
    }

    ::fs::FsDriver* driver = fs::IdentifyFilesystem(device);
    if (!driver) {
        Log::Error("Fs::VolumeManager::Mount: Not filesystem for device!");
        return VolumeErrorInvalidFilesystem;
    }

    return Mount(device, driver, name);
}

// Mount device using driver and optional name
int Mount(FsNode* device, ::fs::FsDriver* driver, const char* name) {
    if (!device->IsCharDevice()) {
        Log::Error("Fs::VolumeManager::Mount: Not a device!");
        return VolumeErrorNotDevice;
    }

    assert(driver->Identify(device)); // Ensure device has right filesystem

    FsVolume* vol = nullptr;
    if (name) {
        assert(strlen(name) < NAME_MAX);

        vol = driver->Mount(device, name); // Name was specified
    } else {
        char name[NAME_MAX]{"volume"};
        itoa(nextVolumeName++, name + strlen("volume"), 10);

        vol = driver->Mount(device, name); // Name was not specified
    }

    if (!vol) {
        Log::Error("fs::VolumeManager::Mount: Unknown driver error mounting volume!");
        return VolumeErrorMisc;
    }

    RegisterVolume(vol);
    return 0;
}

int Unmount(const char* name) {
    for (auto it = volumes->begin(); it != volumes->end(); it++) {
        if (!strcmp((*it)->mountPointDirent.name, name)) {
            UnregisterVolume(*it);
            return 0;
        }
    }

    return VolumeErrorVolumeNotFound;
}

void RegisterVolume(FsVolume* volume) {
    volume->SetVolumeID(nextVolumeID++);
    if(volume->mountPoint){
        volume->mountPoint->parent = fs::GetRoot();
    }
    volumes->add_back(volume);
}

int UnregisterVolume(FsVolume* volume) {
    for (auto it = volumes->begin(); it != volumes->end(); it++) {
        if (*it == volume) {
            volumes->remove(it);
            return 0;
        }
    }

    return VolumeErrorVolumeNotFound;
}

FsVolume* SystemVolume() {
    assert(systemVolume);
    return systemVolume;
}

} // namespace VolumeManager
} // namespace fs