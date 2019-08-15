#include <ahci.h>

#include <pci.h>

namespace AHCI{

	pci_device_t controllerDevice{
		nullptr, // Vendor Pointer
		0, // Device ID
		0, // Vendor ID
		"Generic AHCI Compaitble Storage Controller", // Name
		0, // Bus
		0, // Slot
		PCI_CLASS_STORAGE, // Mass Storage Controller
		0x06, // Serial ATA Controller Subclass
		{},
		{},
		true, // Is a generic driver
	};



	int Init(){
		controllerDevice.classCode = PCI_CLASS_STORAGE;
		controllerDevice.subclass = 0x6;
		controllerDevice = PCI::RegisterPCIDevice(controllerDevice);

		if(controllerDevice.vendorID == 0xFFFF){
			return true; // No AHCI Controller Found
		}

	}
}