#pragma once

#include <stdint.h>
#include <List.h>
#include <Fs/Filesystem.h>
#include <Logging.h>
#include <String.h>

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

enum DeviceBus {
    DeviceBusNone,
    DeviceBusSoftware,
    DeviceBusPCI,
    DeviceBusUSB,
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

    inline bool IsRootDevice() const { return isRootDevice; }

    inline const char* InstanceName() const { return instanceName; }
    inline const char* DeviceName() const { return deviceName; }

    inline DeviceType Type() const { return type; }
    inline int64_t ID() const { return deviceID; }
protected:
    void SetInstanceName(const char* name);
    void SetDeviceName(const char* name);

    bool isRootDevice = true;

    char* instanceName = nullptr;
    char* deviceName = strdup("Unknown Device");

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
    /// \brief Get DevFS
    ///
    /// \return FsNode* object representing devfs root
    /////////////////////////////
    FsNode* GetDevFS();

    /////////////////////////////
    /// \brief Find device from device ID
    ///
    /// \return Corresponding device, nullptr if it does not exist
    /////////////////////////////
    Device* DeviceFromID(int64_t deviceID);

    /////////////////////////////
    /// \brief Find device from path (relative to DevFS root)
    ///
    /// \param path Path of device (relative to DevFS)
    ///
    /// \return Corresponding device, nullptr if it does not exist
    /////////////////////////////
    Device* ResolveDevice(const char* path);

    /////////////////////////////
    /// \brief Get device count
    ///
    /// \return Device count
    /////////////////////////////
    int64_t DeviceCount();

    /////////////////////////////
    /// \brief Enumerate root devices
    ///
    /// Gets a list of device IDs
    ///
    /// \param offset Index of first ID to retrieve
    /// \param count Amount of IDs to retrieve
    /// \param list Buffer to store retireved device IDs
    ///
    /// \return Amount of IDs inserted into list
    /////////////////////////////
    int64_t EnumerateDevices(int64_t offset, int64_t count, int64_t* list);
}