#include <lemon/system/Device.h>

#include <cerrno>

namespace Lemon {
long GetRootDeviceCount() {
    long ret = syscall(SYS_DEVICE_MANAGEMENT, RequestDeviceManagerGetRootDeviceCount);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return ret;
}

long EnumerateRootDevices(int64_t offset, int64_t count, int64_t* buffer) {
    long ret = syscall(SYS_DEVICE_MANAGEMENT, RequestDeviceManagerEnumerateRootDevices, offset, count, buffer);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return ret;
}

long ResolveDevice(const char* path) {
    if (long e = syscall(SYS_DEVICE_MANAGEMENT, RequestDeviceResolveFromPath, path); e) {
        errno = -e;
        return -1;
    }

    return 0;
}

long DeviceGetName(int64_t id, char* name, size_t nameBufferSize) {
    if (long e = syscall(SYS_DEVICE_MANAGEMENT, RequestDeviceGetName, id, name, nameBufferSize); e) {
        errno = -e;
        return -1;
    }

    return 0;
}

long DeviceGetInstanceName(int64_t id, char* name, size_t nameBufferSize) {
    if (long e = syscall(SYS_DEVICE_MANAGEMENT, RequestDeviceGetInstanceName, id, name, nameBufferSize); e) {
        errno = -e;
        return -1;
    }

    return 0;
}

long DeviceGetType(int64_t id) {
    long ret = syscall(SYS_DEVICE_MANAGEMENT, RequestDeviceGetType, id);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return ret;
}
} // namespace Lemon
