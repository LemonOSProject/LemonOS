#include <device.h>

PartitionDevice::PartitionDevice(uint64_t startLBA, uint64_t endLBA, DiskDevice* disk){
    this->startLBA = startLBA;
    this->endLBA = endLBA;
    this->parentDisk = disk;
}

int PartitionDevice::ReadAbsolute(uint64_t offset, uint32_t count, void* buffer){
    if(offset + count > (endLBA - startLBA) * parentDisk->blocksize) return 2;

    uint64_t lba = offset / parentDisk->blocksize;
    uint64_t tempCount = offset + (offset % parentDisk->blocksize); // Account for that we read from start of block
    uint8_t buf[tempCount];

    if(int e = Read(lba, parentDisk->blocksize, buf)){
        return e;
    }

    memcpy(buffer, buf + (offset % parentDisk->blocksize), count);

    return 0;
}

int PartitionDevice::Read(uint64_t lba, uint32_t count, void* buffer){
    if(lba * parentDisk->blocksize + count > (endLBA - startLBA) * parentDisk->blocksize) return 2;

    return parentDisk->Read(lba + startLBA, count, buffer);
}

int PartitionDevice::Write(uint64_t lba, uint32_t count, void* buffer){
    if(lba * parentDisk->blocksize + count > (endLBA - startLBA) * parentDisk->blocksize) return 2;

    return parentDisk->Write(lba + startLBA, count, buffer);
}

PartitionDevice::~PartitionDevice(){
    
}