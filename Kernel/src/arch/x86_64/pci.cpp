#include <pci.h>

#include <system.h>
#include <logging.h>
//#include <list.h>

#define AMD 0x1022
#define INTEL 0x8086
#define NVIDIA 0x10de
#define REALTEK 0x10ec
#define INNOTEK 0x80ee

#define VENDOR_COUNT 5

pci_vendor_t PCIVendors[]{
	{AMD, "Advanced Micro Devices, Inc."},
	{INTEL, "Intel Corporation."},
	{NVIDIA, "NVIDIA"},
	{REALTEK, "Realtek Semiconductor Co., Ltd."},
	{INNOTEK, "Innotek Systemberatung GmbH"},
};

namespace PCI{
	pci_vendor_t vendors[0xFFFF];
	//List<pci_device_t>* devices;

	char* unknownDeviceString = "Unknown Device.";

	void LoadVendorList(){
		for(int i = 0; i < VENDOR_COUNT; i++){
			PCI::RegsiterPCIVendor(PCIVendors[i]);
		}
	}

	uint16_t Config_ReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset){
		uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);

		outportl(0xCF8, address);

		uint16_t data;
		data = (uint16_t)((inportl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);

		return data;
	}

	uint8_t Config_ReadByte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset){
		uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);

		outportl(0xCF8, address);

		uint8_t data;
		data = (uint8_t)((inportl(0xCFC) >> ((offset & 3) * 8)) & 0xff);

		return data;
	}

	pci_device_header_type0_t ReadConfig(uint8_t bus, uint8_t slot){
		pci_device_header_type0_t header;
		uint8_t offset;
		header.headerType = Config_ReadByte(bus, slot, 0, 13) & 0x7F;
		if(header.headerType) return header;
		for(; offset < sizeof(pci_device_header_type0_t); offset += 1){
			*((uint8_t*)(&header) + offset) = Config_ReadByte(bus, slot, 0, offset);
		}
		return header;
	}

	pci_device_header_type1_t ReadType1Config(uint8_t bus, uint8_t slot){
		pci_device_header_type1_t header;
		uint8_t offset;
		header.headerType = Config_ReadByte(bus, slot, 0, 13) & 0x7F;
		if(header.headerType != 1) return header;
		for(; offset < sizeof(pci_device_header_type1_t); offset += 1){
			*((uint8_t*)(&header) + offset) = Config_ReadByte(bus, slot, 0, offset);
		}
	}

	uint16_t GetVendor(uint8_t bus, uint8_t slot){
		uint16_t vendor;

		vendor = Config_ReadWord(bus, slot, 0, 0);
		return vendor;
	}

	uint16_t GetDeviceID(uint8_t bus, uint8_t slot){
		uint16_t id;

		id = Config_ReadWord(bus, slot, 0, 0);
		return id;
	}

	bool CheckDevice(uint8_t bus, uint8_t device){
		if(GetVendor(bus, device) == 0xFFFF){
			return false;
		} else {
			return true;
		}
	}

	void RegsiterPCIVendor(pci_vendor_t vendor){
		vendors[vendor.vendorID] = vendor;
	}

	pci_device_t RegisterPCIDevice(pci_device_t device){
		device.vendor = &vendors[device.vendorID];
/* 
		if(!device.generic){
			for(int i = 0; i < devices->get_length(); i++){
				if(devices->get_at(i).deviceID == device.deviceID && devices->get_at(i).vendorID == device.vendorID){
					pci_device_t dev;
					dev = devices->get_at(i);
					dev.deviceName = device.deviceName;
					dev.generic = device.generic;
					devices->replace_at(i, dev);
					Log::Info("PCI Device Found: ");
					Log::Write(dev.deviceName);
					return dev;
				}
			}
		} else {
			for(int i = 0; i < devices->get_length(); i++){
				if(devices->get_at(i).classCode == device.classCode && devices->get_at(i).subclass == device.subclass){
					pci_device_t dev;
					dev = devices->get_at(i);
					dev.deviceName = device.deviceName;
					dev.generic = device.generic;
					devices->replace_at(i, dev);
					Log::Info("PCI Device Found: ");
					Log::Info(dev.deviceName);
					return dev;
				}
			}
		}*/
		Log::Info("No device found with class");
		Log::Info(device.classCode);
		Log::Info(device.subclass);
		device.vendorID = 0xFFFF;
		return device;
	}

	void Init(){
		//devices = new List<pci_device_t>();

		LoadVendorList();

		for(uint16_t i = 0; i < 256; i++){ // Bus
			for(uint16_t j = 0; j < 32; j++){ // Slot
				if(CheckDevice(i, j)){
					pci_device_t device;

					device.vendorID = GetVendor(i, j);
					device.bus = i;
					device.slot = j;
					device.deviceName = unknownDeviceString;
					
					device.vendor = &vendors[device.vendorID];
					device.header0 = ReadConfig(i, j);
					device.header1 = ReadType1Config(i, j);
					if(device.header0.headerType){
						device.classCode = device.header1.classCode;
						device.subclass = device.header1.subclass;
					} else {
						device.classCode = device.header0.classCode;
						device.subclass = device.header0.subclass;
					}

					//devices->add_back(device);
				}
			}
		}
	}
}