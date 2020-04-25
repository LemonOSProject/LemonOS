#include <ahci.h>

#include <physicalallocator.h>
#include <paging.h>
#include <logging.h>

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

#define ATA_CMD_READ_DMA_EX     0x25
#define HBA_PxIS_TFES   (1 << 30)

namespace AHCI{
	Port::Port(int num, hba_port_t* portStructure){
        registers = portStructure;

        stopCMD(registers);

        uintptr_t phys;
        
		// Command list entry size = 32
		// Command list entry maxim count = 32
		// Command list maxim size = 32*32 = 1K per port
        phys = Memory::AllocatePhysicalMemoryBlock();
		registers->clb = (uint32_t)(phys & 0xFFFFFFFF);
		registers->clbu = (uint32_t)(phys >> 32);
	
		// FIS entry size = 256 bytes per port
        phys = Memory::AllocatePhysicalMemoryBlock();
		registers->fb = (uint32_t)(phys & 0xFFFFFFFF);
		registers->fbu = (uint32_t)(phys >> 32);
	
		// Command list size = 256*32 = 8K per port
		commandList = (hba_cmd_header_t*)Memory::KernelAllocate4KPages(1);
        Memory::KernelMapVirtualMemory4K(registers->clb, (uintptr_t)commandList, 1);
        memset(commandList, 0, PAGE_SIZE_4K);

		// FIS
		fis = Memory::KernelAllocate4KPages(1);
        Memory::KernelMapVirtualMemory4K(registers->fb, (uintptr_t)fis, 1);
        memset(fis, 0, PAGE_SIZE_4K);

        for(int i = 0; i < 8 /*Support for 8 command slots*/; i++){
            commandList[i].prdtl = 8;

            phys = Memory::AllocatePhysicalMemoryBlock();
            commandList[i].ctba = (uint32_t)(phys & 0xFFFFFFFF);
            commandList[i].ctbau = (uint32_t)(phys >> 32);

            commandTables[i] = (hba_cmd_tbl_t*)Memory::KernelAllocate4KPages(1);
            Memory::KernelMapVirtualMemory4K(phys,(uintptr_t)commandTables[i], 1);
            memset(commandTables[i],1,PAGE_SIZE_4K);
        }

        startCMD(registers);

        registers->is = 0;
        registers->ie = 0xFFFFFFFF;
    }

    int Port::Read(uint32_t startl, uint32_t starth, uint32_t count, uint16_t *buf){

    }

    int Port::FindCmdSlot(){
        // If not set in SACT and CI, the slot is free
        uint32_t slots = (registers->sact | registers->ci);
        for (int i=0; i<8; i++)
        {
            if ((slots&1) == 0)
                return i;
            slots >>= 1;
        }

        return -1;
    }
}


