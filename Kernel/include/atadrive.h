#pragma once

#include <stdint.h>
#include <ata.h>
#include <device.h>

namespace ATA{
    class ATADiskDevice : public DiskDevice{
    public:
        ATADiskDevice(int port, int drive); // Constructor
        int Read(uint64_t lba, uint32_t count, void* buffer); // Read bytes
        int Write(uint64_t lba, uint32_t count, void* buffer); // Write bytes

        uint32_t prdBufferPhys;
        uint8_t* prdBuffer;

        uint32_t prdtPhys;
        uint64_t* prdt;
        uint64_t prd;

        int port;
        int drive; // 0 - Master, 1 - Slave
        
        int blocksize = 512;
        
    private:
        char* name = "Generic ATA Disk Device";
    };
}