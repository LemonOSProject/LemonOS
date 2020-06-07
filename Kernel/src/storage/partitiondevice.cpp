#include <device.h>

PartitionDevice::PartitionDevice(uint64_t startLBA, uint64_t endLBA, DiskDevice* disk){
    this->startLBA = startLBA;
    this->endLBA = endLBA;
    this->parentDisk = disk;
}

int PartitionDevice::Read(uint64_t lba, uint32_t count, void* buffer){
    if(count * parentDisk->blocksize > (endLBA - startLBA) * parentDisk->blocksize) return 2;

    return parentDisk->Read(lba + startLBA, count, buffer);
}

PartitionDevice::~PartitionDevice(){
    
}