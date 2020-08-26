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
			Log::Warning("No AHCI Controller Found");
			return true; // No AHCI Controller Found
		}
		

		Log::Info("Initializing AHCI Controller...");

		PCI::Config_WriteWord(controllerDevice.bus, controllerDevice.slot, controllerDevice.func, 0x4, controllerDevice.header0.command | PCI_CMD_BUS_MASTER); // Enable Bus Mastering

		ahciBaseAddress = controllerDevice.header0.baseAddress5 & 0xFFFFFFFFFFFFF000; // BAR 5 is the AHCI Base Address
		ahciVirtualAddress = Memory::GetIOMapping(ahciBaseAddress);

		ahciHBA = (hba_mem_t*)ahciVirtualAddress;

		ahciHBA->ghc |= AHCI_GHC_ENABLE;

		Log::Info("[AHCI]: Base Address: %x, Virtual Base Address: %x", ahciBaseAddress, ahciVirtualAddress);
		Log::Info("[AHCI] Enabled? %Y, BOHC? %Y, 64-bit addressing? %Y, Staggered Spin-up? %Y, Slumber State Capable? %Y, Partial State Capable? %Y, FIS-based switching? %Y", ahciHBA->ghc & AHCI_GHC_ENABLE, ahciHBA->cap2 & AHCI_CAP2_BOHC, ahciHBA->cap & AHCI_CAP_S64A, ahciHBA->cap & AHCI_CAP_SSS, ahciHBA->cap & AHCI_CAP_SSC, ahciHBA->cap & AHCI_CAP_PSC, ahciHBA->cap & AHCI_CAP_FBSS);

		uint32_t pi = ahciHBA->pi;
		for(int i = 0; i < 32; i++){
			if((pi >> i) & 1){
				if(((ahciHBA->ports[i].ssts >> 8) & 0x0F) != HBA_PORT_IPM_ACTIVE || (ahciHBA->ports[i].ssts & 0x0F) != HBA_PORT_DET_PRESENT) continue;

				if(ahciHBA->ports[i].sig == SATA_SIG_ATAPI) ;
				else if(ahciHBA->ports[i].sig == SATA_SIG_PM) ;
				else if(ahciHBA->ports[i].sig == SATA_SIG_SEMB) ;
				else {
					Log::Info("Found SATA Drive - Port: %d", i);

					ports[i] = new Port(i, &ahciHBA->ports[i]);
					Log::Info("name: %s", ports[i]->GetName());
					
					DeviceManager::RegisterDevice(*(ports[i]));
				}
			}
		}

		return 0;
	}
}