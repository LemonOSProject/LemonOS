#include <device.h>

#include <fs/fat32.h>
#include <fs/ext2.h>

int DiskDevice::InitializePartitions(){
    static char letter = 'a';
    for(int i = 0; i < partitions.get_length(); i++){
        if(fs::FAT32::Identify(partitions.get_at(i)) > 0) {
            char name[] =  {'h', 'd', letter++, 0};
            fs::volumes->add_back(new fs::FAT32::Fat32Volume(partitions.get_at(i),name));
        } else if(fs::Ext2::Identify(partitions.get_at(i)) > 0) {
            char name[] =  {'h', 'd', letter++, 0};
            fs::volumes->add_back(new fs::Ext2::Ext2Volume(partitions.get_at(i),name));
        }
    }
    
    return 0;
}

int DiskDevice::Read(uint64_t lba, uint32_t count, void* buffer){
    return -1;
}

DiskDevice::~DiskDevice(){

}