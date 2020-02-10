#pragma once

#include <stdint.h>

#define PCI_BIST_CAPABLE (1 << 7)
#define PCI_BIST_START (1 << 6)
#define PCI_BIST_COMPLETION_CODE (0xF)

#define PCI_CMD_INTERRUPT_DISABLE (1 << 10)
#define PCI_CMD_FAST_BTB_ENABLE (1 << 9)
#define PCI_CMD_SERR_ENABLE (1 << 8)
#define PCI_CMD_PARITY_ERROR_RESPONSE (1 << 6)
#define PCI_CMD_VGA_PALETTE_SNOOP (1 << 5)
#define PCI_CMD_MEMORY_WRITE_INVALIDATE_ENABLE (1 << 4)
#define PCI_CMD_SPECIAL_CYCLES (1 << 3)
#define PCI_CMD_BUS_MASTER (1 << 2)
#define PCI_CMD_MEMORY_SPACE (1 << 1)
#define PCI_CMD_IO_SPACE (1 << 0)

#define PCI_CLASS_UNCLASSIFIED 0x0
#define PCI_CLASS_STORAGE 0x1
#define PCI_CLASS_NETWORK 0x2
#define PCI_CLASS_DISPLAY 0x3
#define PCI_CLASS_MULTIMEDIA 0x4
#define PCI_CLASS_MEMORY 0x5
#define PCI_CLASS_BRIDGE 0x6
#define PCI_CLASS_COMMUNICATION 0x7
#define PCI_CLASS_PERIPHERAL 0x8
#define PCI_CLASS_INPUT_DEVICE 0x9
#define PCI_CLASS_DOCKING_STATION 0xA
#define PCI_CLASS_PROCESSOR 0xB
#define PCI_CLASS_SERIAL_BUS 0xC
#define PCI_CLASS_WIRELESS_CONTROLLER 0xD
#define PCI_CLASS_INTELLIGENT_CONTROLLER 0xE
#define PCI_CLASS_SATELLITE_COMMUNICATION 0xF
#define PCI_CLASS_ENCRYPTON 0x10
#define PCI_CLASS_SIGNAL_PROCESSING 0x11

#define PCI_CLASS_COPROCESSOR 0x40

#define PCI_IO_PORT_CONFIG_ADDRESS 0xCF8
#define PCI_IO_PORT_CONFIG_DATA 0xCFC

typedef struct{
	uint16_t vendorID;
	uint16_t deviceID;
	uint16_t command;
	uint16_t status;
	uint8_t revisionID;
	uint8_t progIF;
	uint8_t subclass;
	uint8_t classCode;
	uint8_t cacheLineSize;
	uint8_t latencyTimer;
	uint8_t headerType;
	uint8_t BIST;
	uint32_t baseAddress0;
	uint32_t baseAddress1;
	uint32_t baseAddress2;
	uint32_t baseAddress3;
	uint32_t baseAddress4;
	uint32_t baseAddress5;
	uint32_t cisPtr;
	uint16_t subsystemVendorID;
	uint16_t subsystemID;
	uint32_t expansionROMBaseAddress;
	uint8_t reserved[3];
	uint8_t capabilitiesPtr;
	uint32_t reserved2;
	uint8_t interruptLine;
	uint8_t interruptPin;
	uint8_t minGrant;
	uint8_t maxLatency;
} __attribute__((packed)) pci_device_header_type0_t;

typedef struct{
	uint16_t vendorID;
	uint16_t deviceID;
	uint16_t command;
	uint16_t status;
	uint8_t revisionID;
	uint8_t progIF;
	uint8_t subclass;
	uint8_t classCode;
	uint8_t cacheLineSize;
	uint8_t latencyTimer;
	uint8_t headerType;
	uint32_t baseAddress0;
	uint32_t baseAddress1;
	uint8_t primaryBusNumber;
	uint8_t secondaryBusNumber;
	uint8_t subordinateBusNumber;
	uint8_t secondaryLatencyTimer;
	uint8_t ioBase;
	uint8_t ioLimit;
	uint16_t secondaryStatus;
	uint16_t memoryLimit;
	uint16_t memoryBase;
	uint16_t prefetchMemoryLimit;
	uint16_t prefetchMemoryBase;
	uint32_t prefetchBaseUpper;
	uint32_t prefetchLimitUpper;
	uint16_t ioLimitUpper;
	uint16_t ioBaseUpper;
	uint8_t reserved[3];
	uint8_t capabilityPointer;
	uint32_t romBaseAddress;
	uint16_t bridgeControl;
	uint8_t interruptPIN;
	uint8_t interruptLine;
} __attribute__((packed)) pci_device_header_type1_t;


typedef struct {
	uint16_t vendorID;
	char* vendorString;
} pci_vendor_t;

typedef struct {
	pci_vendor_t* vendor;

	uint16_t deviceID;
	uint16_t vendorID;

	const char* deviceName;

	uint8_t bus;
	uint8_t slot;

	uint8_t classCode;
	uint8_t subclass;

	pci_device_header_type0_t header0;
	pci_device_header_type1_t header1;

	bool generic = false;
} pci_device_t;

namespace PCI{
	uint16_t ReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
	void Config_WriteWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data);
	void RegsiterPCIVendor(pci_vendor_t vendor);
	pci_device_t RegisterPCIDevice(pci_device_t device);
	bool CheckDevice(uint8_t bus, uint8_t device);
	bool FindDevice(uint16_t bus, uint16_t device);

	void Init();
}