#include <Device.h>

#include <Errno.h>
#include <Fs/Fat32.h>
#include <Fs/VolumeManager.h>
#include <Logging.h>

static int nextDeviceNumber = 0;

DiskDevice::DiskDevice() : Device(DeviceTypeStorageDevice) {
    type = FileType::CharDevice;

    char buf[16];
    strcpy(buf, "hd");
    itoa(nextDeviceNumber++, buf + 2, 10);

    SetInstanceName(buf);
}

int DiskDevice::InitializePartitions() {
    return 0;
}

int DiskDevice::ReadDiskBlock(uint64_t lba, uint32_t count, UIOBuffer* buffer) { return -1; }

int DiskDevice::WriteDiskBlock(uint64_t lba, uint32_t count, UIOBuffer* buffer) { return -1; }

ErrorOr<ssize_t> DiskDevice::Read(size_t off, size_t size, UIOBuffer* buffer) {
    if (off & (blocksize - 1)) {
        return EINVAL; // Block aligned reads only
    }

    int e = ReadDiskBlock(off / blocksize, size, buffer);

    if (e) {
        return EIO;
    }

    return size;
}

ErrorOr<ssize_t> DiskDevice::Write(size_t off, size_t size, UIOBuffer* buffer) { return Error{ENOSYS}; }

DiskDevice::~DiskDevice() {}