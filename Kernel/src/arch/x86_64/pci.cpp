#include <pci.h>

#include <system.h>
#include <logging.h>
#include <vector.h>
#include <acpi.h>
#include <apic.h>
#include <idt.h>

namespace PCI{
	Vector<PCIDevice>* devices;
	PCIDevice* unknownDevice;
	PCIMCFG* mcfgTable;
	PCIConfigurationAccessMode configMode = PCIConfigurationAccessMode::Legacy;
	Vector<PCIMCFGBaseAddress>* enhancedBaseAddresses; // Base addresses for enhanced (PCI Express) configuration mechanism

	uint32_t ConfigReadDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset){
		uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);

		outportl(0xCF8, address);

		uint32_t data;
		data = inportl(0xCFC);

		return data;
	}

	uint16_t ConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset){
		uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);

		outportl(0xCF8, address);

		uint16_t data;
		data = (uint16_t)((inportl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);

		return data;
	}

	uint8_t ConfigReadByte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset){
		uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);

		outportl(0xCF8, address);

		uint8_t data;
		data = (uint8_t)((inportl(0xCFC) >> ((offset & 3) * 8)) & 0xff);

		return data;
	}

	void ConfigWriteDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data){
		uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);

		outportl(0xCF8, address);
		outportl(0xCFC, data);
	}

	void ConfigWriteWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data){
		uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);

		outportl(0xCF8, address);
		outportl(0xCFC, (inportl(0xCFC) & (~(0xFFFF << ((offset & 2) * 8)))) | (static_cast<uint32_t>(data) << ((offset & 2) * 8)));
	}

	void ConfigWriteByte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t data){
		uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);

		outportl(0xCF8, address);
		outportb(0xCFC, data);
	}

	uint16_t GetVendor(uint8_t bus, uint8_t slot, uint8_t func){
		uint16_t vendor;

		vendor = ConfigReadWord(bus, slot, func, 0);
		return vendor;
	}

	uint16_t GetDeviceID(uint8_t bus, uint8_t slot, uint8_t func){
		uint16_t id;

		id = ConfigReadWord(bus, slot, func, 2);
		return id;
	}

	bool CheckDevice(uint8_t bus, uint8_t device, uint8_t func){
		if(GetVendor(bus, device, func) == 0xFFFF){
			return false;
		}

		return true;
	}

	bool FindDevice(uint16_t deviceID, uint16_t vendorID){
		for(unsigned i = 0; i < devices->get_length(); i++){
			if(devices->get_at(i).deviceID == deviceID && devices->get_at(i).vendorID == vendorID){
				return true;
			}
		}

		return false;
	}

	bool FindGenericDevice(uint16_t classCode, uint16_t subclass){
		for(unsigned i = 0; i < devices->get_length(); i++){
			if(devices->get_at(i).classCode == classCode && devices->get_at(i).subclass == subclass){
				return true;
			}
		}

		return false;
	}

	PCIDevice& GetPCIDevice(uint16_t deviceID, uint16_t vendorID){
		for(PCIDevice& dev : *devices){
			if(dev.deviceID == deviceID && dev.vendorID == vendorID){
				return dev;
			}
		}

		Log::Error("No PCI device found with Device ID %x and Vendor ID %x", deviceID, vendorID);
		return *unknownDevice;
	}

	PCIDevice& GetGenericPCIDevice(uint8_t classCode, uint8_t subclass){
		for(PCIDevice& dev : *devices){
			if(dev.classCode == classCode && dev.subclass == subclass){
				return dev;
			}
		}

		Log::Error("No PCI device found with Class %x and Subclass %x", classCode, subclass);
		return *unknownDevice;
	}

	int AddDevice(int bus, int slot, int func){
		PCIDevice device;

		device.vendorID = GetVendor(bus, slot, func);
		device.deviceID = GetDeviceID(bus, slot, func);

		device.bus = bus;
		device.slot = slot;
		device.func = func;

		device.UpdateClass();

		Log::Info("Found Device! Vendor: ");
		Log::Write(device.vendorID);
		Log::Write(", Device: ");
		Log::Write(device.deviceID);
		Log::Write(", Class: ");
		Log::Write(device.classCode);
		Log::Write(", Subclass: ");
		Log::Write(device.subclass);

		device.capabilities = new Vector<uint16_t>();
		if(device.Status() & PCI_STATUS_CAPABILITIES){
			uint8_t ptr = ConfigReadWord(bus, slot, func, PCICapabilitiesPointer) & 0xFC;
			uint16_t cap = ConfigReadDword(bus, slot, func, ptr);
			do {
				Log::Info("PCI Capability: %x", cap & 0xFF);

				if((cap & 0xFF) == PCICapabilityIDs::PCICapMSI){
					device.msiPtr = ptr;
					device.msiCapable = true;
					device.msiCap.register0 = ConfigReadDword(bus, slot, func, ptr);
					device.msiCap.register1 = ConfigReadDword(bus, slot, func, ptr + sizeof(uint32_t));
					device.msiCap.register2 = ConfigReadDword(bus, slot, func, ptr + sizeof(uint32_t) * 2);

					if(device.msiCap.msiControl & PCI_CAP_MSI_CONTROL_64){ // 64-bit capable
						device.msiCap.data64 = ConfigReadDword(bus, slot, func, ptr + sizeof(uint32_t) * 3);
					}
				}

				ptr = (cap >> 8);
				cap = ConfigReadDword(bus, slot, func, ptr);

				device.capabilities->add_back(cap & 0xFF);
			} while((cap >> 8));
		}

		int ret = devices->get_length();
		devices->add_back(device);
		return ret;
	}

	void Init(){
		unknownDevice = new PCIDevice();
		unknownDevice->vendorID = unknownDevice->deviceID = 0xFFFF;
		
		devices = new Vector<PCIDevice>();
		enhancedBaseAddresses = new Vector<PCIMCFGBaseAddress>();

		mcfgTable = ACPI::mcfg;
		if(mcfgTable){
			configMode = PCIConfigurationAccessMode::Enhanced;
			for(unsigned i = 0; i < (mcfgTable->header.length - sizeof(PCIMCFG)) / sizeof(PCIMCFGBaseAddress); i++){
				PCIMCFGBaseAddress& base = mcfgTable->baseAddresses[i];
				if(base.segmentGroupNumber > 0){
					Log::Warning("No support for PCI express segments > 0");
					continue;
				}

				enhancedBaseAddresses->add_back(base);
			}
		}

		for(uint16_t i = 0; i < 256; i++){ // Bus
			for(uint16_t j = 0; j < 32; j++){ // Slot
				if(CheckDevice(i, j, 0)){
					int index = AddDevice(i, j, 0);
					if(devices->get_at(index).HeaderType() & 0x80){
						for(int k = 1; k < 8; k++){ // Func
							if(CheckDevice(i,j,k)){
								AddDevice(i,j,k);
							}
						}
					}

				}
			}
		}
	}
}


uint8_t PCIDevice::AllocateVector(PCIVectors type){
	if(type & PCIVectorMSI){
		if(!msiCapable){
			Log::Error("[PCIDevice] AllocateVector: Device not MSI capable!");
		} else {
			uint8_t interrupt = IDT::ReserveUnusedInterrupt();
			if(interrupt == 0xFF){
				Log::Error("[PCIDevice] AllocateVector: Could not reserve unused interrupt (no free interrupts?)!");
				return interrupt;
			}
			
			msiCap.msiControl = (msiCap.msiControl & ~PCI_CAP_MSI_CONTROL_MME_MASK) | PCI_CAP_MSI_CONTROL_SET_MME(0); // We only support one message at the moment
			msiCap.msiControl |= 1; // Enable MSIs

			msiCap.SetData(ICR_VECTOR(interrupt) | ICR_MESSAGE_TYPE_FIXED);

			msiCap.SetAddress(0);

			PCI::ConfigWriteDword(bus, slot, func, msiPtr, msiCap.register0);
			PCI::ConfigWriteDword(bus, slot, func, msiPtr + sizeof(uint32_t), msiCap.register1);
			PCI::ConfigWriteDword(bus, slot, func, msiPtr + sizeof(uint32_t) * 2, msiCap.register2);

			if(msiCap.msiControl & PCI_CAP_MSI_CONTROL_64){
				PCI::ConfigWriteDword(bus, slot, func, msiPtr + sizeof(uint32_t) * 3, msiCap.register3);
			}

			return interrupt;
		}
	}

	if(type & PCIVectorLegacy){
		uint8_t irq = GetInterruptLine();
		if(irq == 0xFF){
			Log::Error("[PCIDevice] AllocateVector: Legacy interrupts not supported by device.");
			return 0xFF;
		} else {
			APIC::IO::MapLegacyIRQ(irq);
		}

		return IRQ0 + irq;
	}

	Log::Error("[PCIDevice] AllocateVector: Could not allocate interrupt (type %i)!", static_cast<int>(type));
	return 0xFF;
}