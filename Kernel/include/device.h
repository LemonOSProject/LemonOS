#pragma once

#include <stdint.h>
#include <list.h>

enum DeviceType{
    TypeGenericDevice,
    TypeDiskDevice,
    TypePartitionDevice,
};

class Device{
public:
    //Device();
private:
    char* name;
    int type;
};

class PartitionDevice;

class DiskDevice : public Device{
public:
    int InitializePartitions();
    virtual int Read(uint64_t lba, uint16_t count, void* buffer);
    
    List<PartitionDevice*> partitions;
    int blocksize = 512;
private:
    int type = TypeDiskDevice;
};

class PartitionDevice : public Device{
public:
    PartitionDevice(uint64_t startLBA, uint64_t endLBA, DiskDevice* disk);
    virtual int Read(uint64_t lba, uint16_t count, void* buffer);
    
    DiskDevice* parentDisk;
private:

    uint64_t startLBA;
    uint64_t endLBA;

    int type = TypePartitionDevice;
};