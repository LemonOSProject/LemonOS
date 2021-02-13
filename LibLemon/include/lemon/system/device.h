#pragma once

#include <lemon/syscall.h>
#include <stddef.h>

namespace Lemon{
    enum DeviceManagementRequests{
        RequestDeviceManagerGetRootDeviceCount,
        RequestDeviceManagerEnumerateRootDevices,
        RequestDeviceResolveFromPath,
        RequestDeviceGetName,
        RequestDeviceGetInstanceName,
        RequestDeviceGetDescription,
        RequestDeviceGetPCIInformation,
        RequestDeviceIOControl,
        RequestDeviceGetType,
        RequestDeviceGetChildCount,
        RequestDeviceEnumerateChildren,
    };

    enum DeviceType{
        DeviceTypeUnknown,
        DeviceTypeDevFS,
        DeviceTypeUNIXPseudo,
        DeviceTypeUNIXPseudoTerminal,
        DeviceTypeKernelLog,
        DeviceTypeGenericPCI,
        DeviceTypeGenericACPI,
        DeviceTypeStorageController,
        DeviceTypeStorageDevice,
        DeviceTypeStoragePartition,
        DeviceTypeNetworkStack,
        DeviceTypeNetworkAdapter,
        DeviceTypeUSBController,
        DeviceTypeGenericUSB,
        DeviceTypeLegacyHID,
        DeviceTypeUSBHID,
    };

    long GetRootDeviceCount();
    long EnumerateRootDevices(int64_t offset, int64_t count, int64_t* buffer);

    long ResolveDevice(const char* path);
    long DeviceGetName(int64_t id, char* name, size_t nameBufferSize);
    long DeviceGetInstanceName(int64_t id, char* name, size_t nameBufferSize);
    long DeviceGetType(int64_t id);
}