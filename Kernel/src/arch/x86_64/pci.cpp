#include <pci.h>

#include <system.h>
#include <logging.h>
#include <vector.h>

#define AMD 0x1022
#define INTEL 0x8086
#define NVIDIA 0x10de
#define REALTEK 0x10ec
#define INNOTEK 0x80ee

#define VENDOR_COUNT 5

namespace PCI{
	Vector<PCIDevice>* devices;
	PCIDevice* unknownDevice;

	char* unknownDeviceString = "Unknown Device.";

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

	void ConfigWriteWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data){
		uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);

		outportl(0xCF8, address);
		outportw(0xCFC, data);
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

		int ret = devices->get_length();
		devices->add_back(device);
		return ret;
	}

	void Init(){
		unknownDevice = new PCIDevice();
		unknownDevice->vendorID = unknownDevice->deviceID = 0xFFFF;
		
		devices = new Vector<PCIDevice>();

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