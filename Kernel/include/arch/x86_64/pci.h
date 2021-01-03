#pragma once

#include <stdint.h>
#include <assert.h>
#include <vector.h>
#include <list.h>

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

#define PCI_STATUS_CAPABILITIES (1 << 4)

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

#define PCI_SUBCLASS_IDE 0x1
#define PCI_SUBCLASS_FLOPPY 0x2
#define PCI_SUBCLASS_ATA 0x5
#define PCI_SUBCLASS_SATA 0x6
#define PCI_SUBCLASS_NVM 0x8

#define PCI_SUBCLASS_ETHERNET 0x0

#define PCI_SUBCLASS_VGA_COMPATIBLE 0x0
#define PCI_SUBCLASS_XGA 0x1

#define PCI_SUBCLASS_USB 0x3

#define PCI_PROGIF_UHCI 0x20
#define PCI_PROGIF_OHCI 0x10
#define PCI_PROGIF_EHCI 0x20
#define PCI_PROGIF_XHCI 0x30

#define PCI_IO_PORT_CONFIG_ADDRESS 0xCF8
#define PCI_IO_PORT_CONFIG_DATA 0xCFC

#define PCI_CAP_MSI_ADDRESS_BASE 0xFEE00000
#define PCI_CAP_MSI_CONTROL_64 (1 << 7) // 64-bit address capable
#define PCI_CAP_MSI_CONTROL_VECTOR_MASKING (1 << 8) // Enable Vector Masking
#define PCI_CAP_MSI_CONTROL_MME_MASK (0x7U << 4)
#define PCI_CAP_MSI_CONTROL_SET_MME(x) ((x & 0x7) << 4) // Multiple message enable
#define PCI_CAP_MSI_CONTROL_MMC(x) ((x >> 1) & 0x7) // Multiple Message Capable
#define PCI_CAP_MSI_CONTROL_ENABLE (1 << 0) // MSI Enable

enum PCIConfigRegisters{
	PCIDeviceID = 0x2,
	PCIVendorID = 0x0,
	PCIStatus = 0x6,
	PCICommand = 0x4,
	PCIClassCode = 0xB,
	PCISubclass = 0xA,
	PCIProgIF = 0x9,
	PCIRevisionID = 0x8,
	PCIBIST = 0xF,
	PCIHeaderType = 0xE,
	PCILatencyTimer = 0xD,
	PCICacheLineSize = 0xC,
	PCIBAR0 = 0x10,
	PCIBAR1 = 0x14,
	PCIBAR2 = 0x18,
	PCIBAR3 = 0x1C,
	PCIBAR4 = 0x20,
	PCIBAR5 = 0x24,
	PCICardbusCISPointer = 0x28,
	PCISubsystemID = 0x2E,
	PCISubsystemVendorID = 0x2C,
	PCIExpansionROMBaseAddress = 0x30,
	PCICapabilitiesPointer = 0x34,
	PCIMaxLatency = 0x3F,
	PCIMinGrant = 0x3E,
	PCIInterruptPIN = 0x3D,
	PCIInterruptLine = 0x3C,
};

enum PCICapabilityIDs{
	PCICapMSI = 0x5,
};

enum PCIVectors{
	PCIVectorLegacy = 0x1, // Legacy IRQ
	PCIVectorAPIC = 0x2, // I/O APIC
	PCIVectorMSI = 0x4, // Message Signaled Interrupt
	PCIVectorAny = 0x7, // (PCIVectorLegacy | PCIVectorAPIC | PCIVectorMSI)
};

struct PCIMSICapability{
	union{
		struct{
			uint32_t capID : 8; // Should be PCICapMSI
			uint32_t nextCap : 8; // Next Capability
			uint32_t msiControl : 16; // MSI control register
		} __attribute__((packed));
		uint32_t register0;
	};
	union{
		uint32_t addressLow; // Interrupt Message Address Low
		uint32_t register1;
	};
	union{
		uint32_t data; // MSI data
		uint32_t addressHigh; // Interrupt Message Address High (64-bit only)
		uint32_t register2;
	};
	union{
		uint32_t data64; // MSI data when 64-bit capable
		uint32_t register3;
	};

	inline void SetData(uint32_t d){
		if(msiControl & PCI_CAP_MSI_CONTROL_64){
			data64 = d;
		} else {
			data = d;
		}
	}

	inline uint32_t GetData(){
		if(msiControl & PCI_CAP_MSI_CONTROL_64){
			return data64;
		} else {
			return data;
		}
	}

	inline void SetAddress(int cpu){
		addressLow = PCI_CAP_MSI_ADDRESS_BASE | (static_cast<uint32_t>(cpu) << 12);
		if(msiControl & PCI_CAP_MSI_CONTROL_64){
			addressHigh = 0;
		}
	}
} __attribute__((packed));

class PCIDevice;
namespace PCI{
	enum PCIConfigurationAccessMode {
		Legacy, // PCI
		Enhanced, // PCI Express
	};

	uint16_t ReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

	uint32_t ConfigReadDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

	uint16_t ConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
	void ConfigWriteWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data);

	uint8_t ConfigReadByte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
	void ConfigWriteByte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t data);

	bool CheckDevice(uint8_t bus, uint8_t device, uint8_t func);
	bool FindDevice(uint16_t deviceID, uint16_t vendorID);
	bool FindGenericDevice(uint16_t classCode, uint16_t subclass);

	PCIDevice& GetPCIDevice(uint16_t deviceID, uint16_t vendorID);
	PCIDevice& GetGenericPCIDevice(uint8_t classCode, uint8_t subclass);
	List<PCIDevice*> GetGenericPCIDevices(uint8_t classCode, uint8_t subclass);

	void Init();
}

class PCIDevice{
public:
	uint16_t deviceID;
	uint16_t vendorID;
	
	uint8_t bus;
	uint8_t slot;
	uint8_t func;

	uint8_t classCode;
	uint8_t subclass;

	Vector<uint16_t>* capabilities; // List of capability IDs

	uint8_t msiPtr;
	PCIMSICapability msiCap;
	bool msiCapable = false;

	inline uintptr_t GetBaseAddressRegister(uint8_t idx){
		assert(idx >= 0 && idx <= 5);

		uintptr_t bar = PCI::ConfigReadDword(bus, slot, func, PCIBAR0 + (idx * sizeof(uint32_t)));
		if(!(bar & 0x1) /* Not MMIO */ && bar & 0x4 /* 64-bit */ && idx < 5){
			bar |= static_cast<uintptr_t>(PCI::ConfigReadDword(bus, slot, func, PCIBAR0 + ((bar + 1) * sizeof(uint32_t)))) << 32;
		}

		return (bar & 0x1) ? (bar & 0xFFFFFFFFFFFFFFFC) : (bar & 0xFFFFFFFFFFFFFFF0);
	}

	inline bool BarIsIOPort(uint8_t idx){
		return PCI::ConfigReadDword(bus, slot, func, PCIBAR0 + (idx * sizeof(uint32_t))) & 0x1;
	}

	inline uint8_t GetInterruptLine(){
		return PCI::ConfigReadByte(bus, slot, func, PCIInterruptLine);
	}

	inline void SetInterruptLine(uint8_t irq){
		PCI::ConfigWriteByte(bus, slot, func, PCIInterruptLine, irq);
	}

	inline uint16_t GetCommand(){
		return PCI::ConfigReadWord(bus, slot, func, PCICommand);
	}

	inline void SetCommand(uint16_t val){
		PCI::ConfigWriteWord(bus, slot, func, PCICommand, val);
	}

	inline void EnableBusMastering(){
		PCI::ConfigWriteWord(bus, slot, func, PCICommand, PCI::ConfigReadWord(bus, slot, func, PCICommand) | PCI_CMD_BUS_MASTER);
	}

	inline void EnableInterrupts(){
		PCI::ConfigWriteWord(bus, slot, func, PCICommand, PCI::ConfigReadWord(bus, slot, func, PCICommand) & (~PCI_CMD_INTERRUPT_DISABLE));
	}

	inline void EnableMemorySpace(){
		PCI::ConfigWriteWord(bus, slot, func, PCICommand, PCI::ConfigReadWord(bus, slot, func, PCICommand) | PCI_CMD_MEMORY_SPACE);
	}

	inline void EnableIOSpace(){
		PCI::ConfigWriteWord(bus, slot, func, PCICommand, PCI::ConfigReadWord(bus, slot, func, PCICommand) | PCI_CMD_IO_SPACE);
	}

	inline void UpdateClass(){
		classCode = PCI::ConfigReadByte(bus, slot, func, PCIClassCode);
		subclass = PCI::ConfigReadByte(bus, slot, func, PCISubclass);
	}

	inline uint8_t HeaderType(){
		return PCI::ConfigReadByte(bus, slot, func, PCIHeaderType);
	}

	inline uint8_t GetProgIF(){
		return PCI::ConfigReadByte(bus, slot, func, PCIProgIF);
	}

	inline uint16_t Status(){
		return PCI::ConfigReadWord(bus, slot, func, PCIStatus);
	}

	inline bool HasCapability(uint16_t capability){
		return false;
	}

	uint8_t AllocateVector(PCIVectors type);
};