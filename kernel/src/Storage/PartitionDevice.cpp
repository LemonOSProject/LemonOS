#include <Device.h>

#include <CString.h>
#include <Errno.h>

PartitionDevice::PartitionDevice(uint64_t startLBA, uint64_t endLBA, DiskDevice* disk)
    : Device(DeviceTypeStoragePartition) {
    m_startLBA = startLBA;
    m_endLBA = endLBA;
    this->parentDisk = disk;

    type = FileType::CharDevice;

    char buf[18];
    strcpy(buf, parentDisk->InstanceName().c_str());
    strcat(buf, "p");
    itoa(parentDisk->nextPartitionNumber++, buf + strlen(buf), 10);

    SetInstanceName(buf);
}

int PartitionDevice::ReadAbsolute(uint64_t offset, uint32_t count, UIOBuffer* buffer) {
    if (offset + count > (m_endLBA - m_startLBA) * parentDisk->blocksize)
        return 2;

    uint64_t lba = offset / parentDisk->blocksize;
    uint64_t tempCount = offset + (offset % parentDisk->blocksize); // Account for that we read from start of block
    uint8_t buf[tempCount];

    if (int e = ReadBlockRaw(lba, parentDisk->blocksize, buf)) {
        return e;
    }

    if(buffer->write(buf + (offset % parentDisk->blocksize), count)) {
        return EFAULT;
    }

    return 0;
}

int PartitionDevice::ReadBlock(uint64_t lba, uint32_t count, UIOBuffer* buffer) {
    if (lba * parentDisk->blocksize + count > (m_endLBA - m_startLBA) * parentDisk->blocksize) {
        Log::Debug(debugLevelPartitions, DebugLevelNormal,
                   "[PartitionDevice] ReadBlock: LBA %x out of partition range!", lba + count / parentDisk->blocksize);
        return 2;
    }

    return parentDisk->ReadDiskBlock(lba + m_startLBA, count, buffer);
}

int PartitionDevice::WriteBlock(uint64_t lba, uint32_t count, UIOBuffer* buffer) {
    if (lba * parentDisk->blocksize + count > (m_endLBA - m_startLBA) * parentDisk->blocksize)
        return 2;

    return parentDisk->WriteDiskBlock(lba + m_startLBA, count, buffer);
}

ErrorOr<ssize_t> PartitionDevice::read(size_t off, size_t size, UIOBuffer* buffer) {
    if (off & (parentDisk->blocksize - 1)) {
        Log::Warning("PartitionDevice::Read: Unaligned offset %d!", off);
        return Error{EINVAL}; // Block aligned reads only}
    }

    int e = parentDisk->ReadDiskBlock(m_startLBA + off / parentDisk->blocksize, size, buffer);

    if (e) {
        return Error{EIO};
    }

    return size;
}

ErrorOr<ssize_t> PartitionDevice::write(size_t off, size_t size, UIOBuffer* buffer) {
    if (off & (parentDisk->blocksize - 1)) {
        Log::Warning("PartitionDevice::Write: Unaligned offset %d!", off);
        return Error{EINVAL}; // Block aligned writes only}
    }

    int e = parentDisk->WriteDiskBlock(m_startLBA + off / parentDisk->blocksize, size, buffer);

    if (e) {
        return Error{EIO};
    }

    return size;
}

PartitionDevice::~PartitionDevice() {}