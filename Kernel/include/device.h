#pragma once

#include <stdint.h>
#include <list.h>
#include <fs/filesystem.h>
#include <logging.h>
#include <string.h>

struct DevicePCIInformation {
    uint16_t vendorID; // PCI Vendor ID
    uint16_t deviceID; // PCI Device ID

    uint8_t bus; // PCI Bus
    uint8_t slot; // PCI Slot
    uint8_t func; // PCI Func

    uint8_t classCode; // PCI Class
    uint8_t subclass; // PCI Subclass
    uint8_t progIf; // PCI Programming Interface

    uintptr_t bars[6]; // PCI BARs
};

enum DeviceType{
    DeviceTypeUnknown,
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

class Device : public FsNode {
public:
    Device(DeviceType type);
    Device(const char* name, DeviceType type);
    Device(DeviceType type, Device* parent);
    Device(const char* name, DeviceType type, Device* parent);

    ~Device();

    inline void SetID(int64_t id){
        assert(deviceID == 0); // Device ID should only be set once

        deviceID = id;
    }

    inline const char* GetName() const {
        return name;
    }

    inline int64_t ID() const { return deviceID; }
    inline bool IsRootDevice() const { return isRootDevice; }
protected:
    void SetName(const char*);

    inline void SetDescription(const char* desc){
        this->description = strdup(desc);
    }

    bool isRootDevice = true;

    char* name = nullptr;
    char* description = nullptr;

    DeviceType type = DeviceTypeUnknown;

    int64_t deviceID = 0;

    static int nextUnnamedDeviceNumber; // Used when a name was not provided
};

class PartitionDevice;

class DiskDevice : public Device{
    friend class PartitionDevice;
protected:
    int nextPartitionNumber = 0;
public:
    DiskDevice();

    int InitializePartitions();

    virtual int ReadDiskBlock(uint64_t lba, uint32_t count, void* buffer);
    virtual int WriteDiskBlock(uint64_t lba, uint32_t count, void* buffer);

    virtual ssize_t Read(size_t off, size_t size, uint8_t* buffer);
    virtual ssize_t Write(size_t off, size_t size, uint8_t* buffer);

    virtual ~DiskDevice();
    
    List<PartitionDevice*> partitions;
    int blocksize = 512;
private:
};

class PartitionDevice : public Device{
public:
    PartitionDevice(uint64_t startLBA, uint64_t endLBA, DiskDevice* disk);

    virtual int ReadAbsolute(uint64_t off, uint32_t count, void* buffer);
    virtual int ReadBlock(uint64_t lba, uint32_t count, void* buffer);
    virtual int WriteBlock(uint64_t lba, uint32_t count, void* buffer);
    
    virtual ~PartitionDevice();

    DiskDevice* parentDisk;
private:

    uint64_t startLBA;
    uint64_t endLBA;
};

namespace DeviceManager{

    enum DeviceManagementRequests{
        RequestDeviceManagerGetDeviceCount,
        RequestDeviceManagerEnumerateDevices,
        RequestDeviceResolveFromPath,
        RequestDeviceGetName,
        RequestDeviceGetDescription,
        RequestDeviceGetPCIInformation,
        RequestDeviceIOControl,
        RequestDeviceGetType,
        RequestDeviceGetChildCount,
        RequestDeviceEnumerateChildren,
    };

    void Initialize();
    
    /////////////////////////////
    /// \brief Initialize basic devices (null, urandom, etc.)
    /////////////////////////////
    void InitializeBasicDevices();

    /////////////////////////////
    /// \brief Register a device
    ///
    /// Register a device and assign an ID
    ///
    /// \param dev reference to Device to add
    /////////////////////////////
    void RegisterDevice(Device* dev);

    /////////////////////////////
    /// \brief Unregister a device
    ///
    /// Remove a device from the device map. Remove device from devfs if nescessary.
    ///
    /// \param dev reference to Device to add
    /////////////////////////////
    void UnregisterDevice(Device* dev);

    /////////////////////////////
    /// \brief Get devfs
    ///
    /// \return FsNode* object representing devfs root
    /////////////////////////////
    FsNode* GetDevFS();
}