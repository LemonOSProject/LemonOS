#include <Storage/ATADrive.h>

#include <Errno.h>
#include <Logging.h>
#include <Memory.h>
#include <Paging.h>
#include <PhysicalAllocator.h>
#include <Storage/GPT.h>

namespace ATA {
ATADiskDevice::ATADiskDevice(int port, int drive) {
    this->port = port;
    this->drive = drive;

    prdBufferPhys = Memory::AllocatePhysicalMemoryBlock();
    prdBuffer = (uint8_t*)Memory::KernelAllocate4KPages(1);

    prdtPhys = Memory::AllocatePhysicalMemoryBlock();
    prdt = (uint64_t*)Memory::GetIOMapping(prdtPhys);

    Memory::KernelMapVirtualMemory4K(prdBufferPhys, (uintptr_t)prdBuffer, 1);
    Memory::KernelMapVirtualMemory4K(prdtPhys, (uintptr_t)prdt, 1);

    prd = ATA_PRD_BUFFER((uint64_t)prdBufferPhys) | ((uint64_t)PAGE_SIZE_4K << 32) |
          ATA_PRD_END; // Assign the buffer, transfer size (4K) and designate the PRDT entry as the last one
    *prdt = prd;

    switch (GPT::Parse(this)) {
    case 0:
        Log::Error("ATA Disk ");
        Log::Write(this->port ? "Secondary " : "Primary ");
        Log::Write(this->drive ? "Slave " : "Master ");
        Log::Write("has a corrupted or non-existant GPT. MBR disks are NOT supported.");
        break;
    case -1:
        Log::Error("Disk Error while Parsing GPT for ATA Disk ");
        Log::Write(this->port ? "Secondary " : "Primary ");
        Log::Write(this->drive ? "Slave " : "Master ");
        break;
    }

    if (port) {
        if (drive) {
            SetDeviceName("ATA Secondary Slave");
        } else {
            SetDeviceName("ATA Secondary Master");
        }
    } else {
        if (drive) {
            SetDeviceName("ATA Primary Slave");
        } else {
            SetDeviceName("ATA Primary Master");
        }
    }

    InitializePartitions();
}

int ATADiskDevice::ReadDiskBlock(uint64_t lba, uint32_t count, UIOBuffer* buffer) {
    uint64_t blockCount = ((count / 512 * 512) < count) ? ((count / 512) + 1) : (count / 512);

    while (blockCount-- && count) {
        uint64_t size;
        if (count < 512)
            size = count;
        else
            size = 512;

        if (!size)
            continue;

        if (driveLock.Wait()) {
            return -EINTR;
        }

        if (ATA::Access(this, lba, 1, false)) {
            driveLock.Signal();
            return 1; // Error Reading Sectors
        }

        if(buffer->Write((uint8_t*)prdBuffer, size)) {
            return EFAULT;
        }

        driveLock.Signal();

        buffer += size;
        lba++;
    }

    return 0;
}

int ATADiskDevice::WriteDiskBlock(uint64_t lba, uint32_t count, UIOBuffer* buffer) {
    uint64_t blockCount = ((count / 512 * 512) < count) ? ((count / 512) + 1) : (count / 512);

    while (blockCount-- && count) {
        uint64_t size;
        if (count < 512)
            size = count;
        else
            size = 512;

        if (!size)
            continue;

        if (driveLock.Wait()) {
            return EINTR;
        }

        if(buffer->Read((uint8_t*)prdBuffer, size)) {
            return EFAULT;
        }

        if (ATA::Access(this, lba, 1, true)) {
            driveLock.Signal();
            return 1; // Error Reading Sectors
        }
        driveLock.Signal();

        buffer += size;
        lba++;
    }

    return 0;
}
} // namespace ATA