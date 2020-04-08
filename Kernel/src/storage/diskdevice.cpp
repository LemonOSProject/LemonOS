#include <device.h>

#include <fat32.h>

int DiskDevice::InitializePartitions(){
    char letter = 'a';
    for(int i = 0; i < partitions.get_length(); i++){
        if(fs::FAT32::Identify(partitions.get_at(i))) {
            char name[] =  {'h', 'd', letter++, 0};
            fs::volumes->add_back(new fs::FAT32::Fat32Volume(partitions.get_at(i),name));
        }
    }
    
    return 0;
}

int DiskDevice::Read(uint64_t lba, uint16_t count, void* buffer){
    
}