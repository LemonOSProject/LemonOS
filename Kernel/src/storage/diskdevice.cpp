#include <device.h>

#include <fs/fat32.h>
#include <fs/ext2.h>
#include <logging.h>
#include <errno.h>

static int nextDeviceNumber = 0;

DiskDevice::DiskDevice() : Device(DeviceTypeStorageDevice){
    flags = FS_NODE_CHARDEVICE;

    char buf[16];
    strcpy(buf, "hd");
    itoa(nextDeviceNumber++, buf + 2, 10);

    SetInstanceName(buf);
}

int DiskDevice::InitializePartitions(){
    static char letter = 'a';
    for(unsigned i = 0; i < partitions.get_length(); i++){
        if(fs::FAT32::Identify(partitions.get_at(i)) > 0) {
            char vname[] =  {'h', 'd', letter++, 0};
            auto vol = new fs::FAT32::Fat32Volume(partitions.get_at(i),vname);
            fs::volumes->add_back(vol);
        } else if(fs::Ext2::Identify(partitions.get_at(i)) > 0) {
            bool systemFound = false;

            for(auto& vol : *fs::volumes){ // First Ext2 partition is mounted as /system
                if(strcmp(vol->mountPointDirent.name, "system") == 0){
                    systemFound = true; // System partition found
                }
            }

            fs::Ext2::Ext2Volume* vol = nullptr;

            if(systemFound){
                char vname[] = {'h', 'd', letter++, 0};
                vol = new fs::Ext2::Ext2Volume(partitions.get_at(i), vname);
            } else {
                vol = new fs::Ext2::Ext2Volume(partitions.get_at(i), "system");
            }

            if(!vol->Error()){
                fs::volumes->add_back(vol);
                //fs::volumes->add_back(new fs::LinkVolume(fs::FindDir(vol->mountPoint, "lib"), "lib"));
            } else {
                delete vol;
            }
        }
    }
    
    return 0;
}

int DiskDevice::ReadDiskBlock(uint64_t lba, uint32_t count, void* buffer){
    return -1;
}

int DiskDevice::WriteDiskBlock(uint64_t lba, uint32_t count, void* buffer){
    return -1;
}

ssize_t DiskDevice::Read(size_t off, size_t size, uint8_t* buffer){
    if(off % blocksize){
        return -EINVAL; // Block aligned reads only
    }

    int e = ReadDiskBlock(off / blocksize, size, buffer);

    if(e){
        return -EIO;
    }

    return size;
}

ssize_t DiskDevice::Write(size_t off, size_t size, uint8_t* buffer){
    return -ENOSYS;
}

DiskDevice::~DiskDevice(){

}