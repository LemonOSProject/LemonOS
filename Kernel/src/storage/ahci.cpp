#include <ahci.h>

#include <pci.h>
#include <paging.h>
#include <logging.h>
#include <memory.h>

namespace AHCI{

	struct AHCIDevice{
		hba_port_t* port;
		bool connected;
	};

	uintptr_t ahciBaseAddress;
	uintptr_t ahciVirtualAddress;
	hba_mem_t* ahciHBA;

	Port* ports[32];

	pci_device_t controllerDevice{
		nullptr, // Vendor Pointer
		0, // Device ID
		0, // Vendor ID
		"Generic AHCI Compaitble Storage Controller", // Name
		0, // Bus
		0, // Slot
		0, // Func
		PCI_CLASS_STORAGE, // Mass Storage Controller
		0x06, // Serial ATA Controller Subclass
		{},
		{},
		true, // Is a generic driver
	};

	int Init(){
		controllerDevice.classCode = PCI_CLASS_STORAGE; // Storage Device
		controllerDevice.subclass = 0x6; // AHCI Controller
		controllerDevice = PCI::RegisterPCIDevice(controllerDevice);

		if(controllerDevice.vendorID == 0xFFFF){
			return true; // No AHCI Controller Found
		}

		Log::Info("Initializing AHCI Controller...");

		PCI::Config_WriteWord(controllerDevice.bus, controllerDevice.slot, controllerDevice.func, 0x4, controllerDevice.header0.command | PCI_CMD_BUS_MASTER); // Enable Bus Mastering

		ahciBaseAddress = controllerDevice.header0.baseAddress5 & 0xFFFFFFFFFFFFF000; // BAR 5 is the AHCI Base Address
		ahciVirtualAddress = Memory::GetIOMapping(ahciBaseAddress);

		ahciHBA = (hba_mem_t*)ahciVirtualAddress;

		Log::Info("[AHCI]: Base Address: ");
		Log::Write(ahciBaseAddress);
		Log::Write(", Virtual Base Address:");
		Log::Write(ahciVirtualAddress);

		uint32_t pi = ahciHBA->pi;
		Log::Info(pi);

		return 0;
		
		for(int i = 0; i < 32; i++){
			if(pi & 1){
				if((ahciHBA->ports[i].ssts >> 8) & 0x0F != HBA_PORT_IPM_ACTIVE || ahciHBA->ports[i].ssts & 0x0F != HBA_PORT_DET_PRESENT) continue;

				if(ahciHBA->ports[i].sig == SATA_SIG_ATAPI) ;
				else if(ahciHBA->ports[i].sig == SATA_SIG_PM) ;
				else if(ahciHBA->ports[i].sig == SATA_SIG_SEMB) ;
				else {
					Log::Info("Found SATA Drive - Port: ");
					Log::Write(i);

					Log::Info(ahciHBA->ports[i].sig);

					ports[i] = new Port(i, &ahciHBA->ports[i]);
					//uint8_t* buffer = (uint8_t*)kmalloc(4096*4);
					//ports[i]->Read(0, 4, 4, (uint16_t*)buffer);

					/*for(int i = 0; i < 0x1000*4; i++){
						Log::Write(buffer[i]);
						Log::Write(", ");
					}*/
				}
			}

			pi >>= 1;
		}
	}
}