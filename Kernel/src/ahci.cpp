#include <ahci.h>

#include <pci.h>
#include <paging.h>
#include <logging.h>
#include <memory.h>

#define SATA_SIG_SATA 0x00000101

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

namespace AHCI{
	struct AHCIDevice{
		hba_port_t* port;
		bool connected;
	};

	uint32_t ahciBaseAddress;
	uintptr_t ahciVirtualAddress;
	hba_mem_t* ahciHBA;

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

	void startCMD(hba_port_t *port)
	{
		// Wait until CR (bit15) is cleared
		while (port->cmd & HBA_PxCMD_CR)
			;
	
		// Set FRE (bit4) and ST (bit0)
		port->cmd |= HBA_PxCMD_FRE;
		port->cmd |= HBA_PxCMD_ST; 
	}
	
	// Stop command engine
	void stopCMD(hba_port_t *port)
	{
		// Clear ST (bit0)
		port->cmd &= ~HBA_PxCMD_ST;
	
		// Wait until FR (bit14), CR (bit15) are cleared
		while(1)
		{
			if (port->cmd & HBA_PxCMD_FR)
				continue;
			if (port->cmd & HBA_PxCMD_CR)
				continue;
			break;
		}
	
		// Clear FRE (bit4)
		port->cmd &= ~HBA_PxCMD_FRE;
	}

	void portRebase(hba_port_t *port, int portno)
	{
		stopCMD(port);	// Stop command engine
	
		// Command list offset: 1K*portno
		// Command list entry size = 32
		// Command list entry maxim count = 32
		// Command list maxim size = 32*32 = 1K per port
		//port->clb = ahciVirtualAddress + (portno<<10);
		//port->clbu = 0;
		//memset((void*)(port->clb), 0, 1024);

		return;
	
		// FIS offset: 32K+256*portno
		// FIS entry size = 256 bytes per port
		port->fb = ahciVirtualAddress + (32<<10) + (portno<<8);
		port->fbu = 0;
		memset((void*)(port->fb), 0, 256);
	
		// Command table offset: 40K + 8K*portno
		// Command table size = 256*32 = 8K per port
		hba_cmd_header_t *cmdheader = (hba_cmd_header_t*)(port->clb);
		for (int i=0; i<32; i++)
		{
			cmdheader[i].prdtl = 8;	// 8 prdt entries per command table
						// 256 bytes per command table, 64+16+48+16*8
			// Command table offset: 40K + 8K*portno + cmdheader_index*256
			cmdheader[i].ctba = ahciVirtualAddress + (40<<10) + (portno<<13) + (i<<8);
			cmdheader[i].ctbau = 0;
			memset((void*)cmdheader[i].ctba, 0, 256);
		}
	
		startCMD(port);	// Start command engine
	}

	int Init(){
		controllerDevice.classCode = PCI_CLASS_STORAGE; // Storage Device
		controllerDevice.subclass = 0x6; // AHCI Controller
		controllerDevice = PCI::RegisterPCIDevice(controllerDevice);

		if(controllerDevice.vendorID == 0xFFFF){
			return true; // No AHCI Controller Found
		}

		Log::Info("Initializing AHCI Controller...");

		ahciBaseAddress = controllerDevice.header0.baseAddress5 & 0xFFFFF000; // BAR 5 is the AHCI Base Address
		ahciVirtualAddress = (uintptr_t)Memory::KernelAllocate4KPages(1);
		Memory::KernelMapVirtualMemory4K(ahciBaseAddress, (uintptr_t)ahciVirtualAddress, 1); // Map the AHCI Base Address into virtual memory

		ahciHBA = (hba_mem_t*)ahciVirtualAddress;

		uint32_t pi = ahciHBA->pi;
		for(int i = 0; i < 32; i++){
			if(pi & 1){
				if(ahciHBA->ports[i].sig == SATA_SIG_SATA){
					Log::Info("Found SATA Drive - Port: ");
					Log::Write(i);

					//portRebase(&ahciHBA->ports[i],i);
				}
			}

			pi >>= 1;
		}
	}
}