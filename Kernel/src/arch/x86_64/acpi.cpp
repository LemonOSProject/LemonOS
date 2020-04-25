#include <acpi.h>

#include <string.h>
#include <logging.h>
#include <panic.h>
#include <paging.h>
#include <pci.h>
#include <liballoc.h>
#include <timer.h>

#include <lai/core.h>
#include <lai/helpers/pm.h>
#include <lai/helpers/sci.h>

namespace ACPI{
	const char* signature = "RSD PTR ";

	RSDPDescriptor desc;
	RSDTHeader rsdtHeader;

	char oem[7];

	void *FindSDT(const char* signature, int index)
	{
		int entries = (rsdtHeader.length - sizeof(RSDTHeader)) / 4;
		int _index = 0;
	
		for (int i = 0; i < entries; i++)
		{
			SDTHeader* h = (SDTHeader*)Memory::GetIOMapping(rsdtHeader.sdtPointers[i]);
			if (memcmp(h->signature, signature, 4) == 0 && _index++ == index)
				return h;
		}
	
		// No SDT found
		return NULL;
	}

	void Init(){
		for(int i = 0; i <= 0x400; i++){ // Search first KB for RSDP
			if(memcmp((void*)i,signature,8) == 0){
				desc = *((RSDPDescriptor*)Memory::GetIOMapping(i));

				goto success;
			}
		}

		for(int i = 0xE0000; i <= 0xFFFFF; i++){ // Search further for RSDP
			if(memcmp((void*)i,signature,8) == 0){
				desc = *((RSDPDescriptor*)Memory::GetIOMapping(i));

				goto success;
			}
		}

		{
		const char* panicReasons[]{"System not ACPI Complaiant."};
		KernelPanic(panicReasons,1);
		}

		success:

		rsdtHeader = *((RSDTHeader*)desc.rsdtAddress);

		memcpy(oem,rsdtHeader.oemID,6);
		oem[6] = 0; // Zero OEM String

		Log::Info("ACPI Revision: ");
		Log::Info(rsdtHeader.revision, false);
		Log::Info("OEM ID: ");
		Log::Write(oem);

		lai_set_acpi_revision(rsdtHeader.revision);
		//lai_create_namespace();
		//lai_enable_acpi(0);
	}

	void Reset(){
		lai_acpi_reset();
	}
}

extern "C"{
	void *laihost_scan(const char* signature, size_t index){
		return ACPI::FindSDT(signature, index);
	}

	void laihost_log(int level, const char *msg){
		switch(level){
			case LAI_WARN_LOG:
				Log::Warning(msg);
				break;
			default:
				Log::Info(msg);
				break;
		}
	}

	/* Reports a fatal error, and halts. */
	__attribute__((noreturn)) void laihost_panic(const char *msg){
		const char* panicReasons[]{"ACPI Error:", msg};
		KernelPanic(panicReasons,2);

		for(;;);
	}
	
	void *laihost_malloc(size_t sz){
		return kmalloc(sz);
	}

	void *laihost_realloc(void* addr, size_t sz){
		return krealloc(addr, sz);
	}
	void laihost_free(void * addr){
		kfree(addr);
	}

	void *laihost_map(size_t address, size_t count){
		void* virt = Memory::KernelAllocate4KPages(count / PAGE_SIZE_4K + 1);
		
		Memory::KernelMapVirtualMemory4K(address, (uintptr_t)virt, count / PAGE_SIZE_4K + 1);

		return virt;
	}

	void laihost_unmap(void* ptr, size_t count){
		// stub
	}

	void laihost_outb(uint16_t port, uint8_t val){
		outportb(port, val);
	}

	void laihost_outw(uint16_t port, uint16_t val){
		outportw(port, val);
	}

	void laihost_outd(uint16_t port, uint32_t val){
		outportd(port, val);
	}

	uint8_t laihost_inb(uint16_t port){
		return inportb(port);
	}

	uint16_t laihost_inw(uint16_t port){
		return inportw(port);
	}

	uint32_t laihost_ind(uint16_t port){
		return inportd(port);
	}
		
	void laihost_sleep(uint64_t ms){
		uint64_t freq = Timer::GetFrequency();
		float delayInTicks = (freq/1000)*ms;
		uint64_t seconds = Timer::GetSystemUptime();
		uint64_t ticks = Timer::GetTicks();
		uint64_t totalTicks = seconds*freq + ticks;

		for(;;){
			uint64_t totalTicksNew = Timer::GetSystemUptime()*freq + Timer::GetTicks();
			if(totalTicksNew - totalTicks == (int)delayInTicks){
				break;
			}
		}
		return;
	}

	/* Write a byte/word/dword to the given device's PCI configuration space
   at the given offset. */
/*void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val){
	
}*/

void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint16_t val){
	PCI::Config_WriteWord(bus, slot, fun, offset, val);
}

/*void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint32_t val){

}

/* Read a byte/word/dword from the given device's PCI configuration space
   at the given offset. * /
uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset){

}

uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset){

}

uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset){

}*/

}