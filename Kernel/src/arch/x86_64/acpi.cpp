#include <acpi.h>

#include <string.h>
#include <logging.h>
#include <panic.h>
#include <paging.h>
#include <pci.h>
#include <liballoc.h>
#include <timer.h>
#include <apic.h>
#include <list.h>

#include <lai/core.h>
#include <lai/helpers/pm.h>
#include <lai/helpers/sci.h>

namespace ACPI{
	uint8_t processors[256];
	int processorCount = 1;

	const char* signature = "RSD PTR ";

	List<apic_iso_t*>* isos;

	acpi_rsdp_t* desc;
	acpi_rsdt_t* rsdtHeader;
	acpi_xsdt_t* xsdtHeader;
	acpi_fadt_t* fadt;

	char oem[7];

	void* FindSDT(const char* signature, int index)
	{
		int entries = (rsdtHeader->header.length - sizeof(acpi_header_t)) / 4;
		uint32_t* sdtPointers = rsdtHeader->tables;//(uint32_t*)(((uintptr_t)rsdtHeader) + sizeof(RSDTHeader));
		int _index = 0;

		Log::Info("Finding: ");
		Log::Write(signature);

		if(memcmp("DSDT", signature, 4) == 0) return (void*)Memory::GetIOMapping(fadt->dsdt);
	
		for (int i = 0; i < entries; i++)
		{
			acpi_header_t* h = (acpi_header_t*)Memory::GetIOMapping(sdtPointers[i]);
			if (memcmp(h->signature, signature, 4) == 0 && _index++ == index)
				return h;
		}
	
		// No SDT found
		return NULL;
	}

	int ReadMADT(){
		void* madt = FindSDT("APIC", 0);

		if(!madt){
			Log::Error("Could Not Find MADT");
			return 1;
		}

		acpi_madt_t* madtHeader = (acpi_madt_t*)madt;
		void* madtEnd = madt + madtHeader->header.length;

		void* madtEntry = madt + sizeof(acpi_madt_t);

		while(madtEntry < madtEnd){
			acpi_madt_entry_t* entry = (acpi_madt_entry_t*)madtEntry;

			switch(entry->type){
			case 0:
				{
					apic_local_t* localAPIC = (apic_local_t*)entry;

					if(((apic_local_t*)entry)->flags & 0x3) {
						if(localAPIC->apicID == 0) break; // Found the BSP
						
						processors[processorCount++] = localAPIC->apicID;
						Log::Info("[ACPI] Found Processor, APIC ID: %d", localAPIC->apicID);
					}
				}
				break;
			case 1:
				{
					apic_io_t* ioAPIC = (apic_io_t*)entry;
					Log::Info("[ACPI] Found I/O APIC, Address: %x", ioAPIC->address);
					if(!ioAPIC->gSIB)
						APIC::IO::SetBase(ioAPIC->address);
				}
				break;
			case 2:
				{
					apic_iso_t* interruptSourceOverride = (apic_iso_t*)entry;
					isos->add_back(interruptSourceOverride);
				}
				break;
			case 4:
				{
					apic_nmi_t* nonMaskableInterrupt = (apic_nmi_t*)entry;
					Log::Info("[ACPI] Found NMI, LINT #%d", nonMaskableInterrupt->lINT);
				}
				break;
			case 5:
				//apic_local_address_override_t* addressOverride = (apic_local_address_override_t*)entry;
				break;
			default:
				Log::Error("Invalid MADT Entry, Type: %d", entry->type);
				break;
			}

			madtEntry += entry->length;
		}

		Log::Info("[ACPI] System Contains %d Processors", processorCount);

		return 0;
	}

	void Init(){
		if(desc){
			goto success; // Already found
		}

		for(int i = 0; i <= 0x7BFF; i += 16){ // Search first KB for RSDP, the RSDP is aligned on a 16 byte boundary
			if(memcmp((void*)Memory::GetIOMapping(i),signature,8) == 0){
				desc =  ((acpi_rsdp_t*)Memory::GetIOMapping(i));

				goto success;
			}
		}

		for(int i = 0x80000; i <= 0x9FFFF; i += 16){ // Search further for RSDP
			if(memcmp((void*)Memory::GetIOMapping(i),signature,8) == 0){
				desc = ((acpi_rsdp_t*)Memory::GetIOMapping(i));

				goto success;
			}
		}

		for(int i = 0xE0000; i <= 0xFFFFF; i += 16){ // Search further for RSDP
			if(memcmp((void*)Memory::GetIOMapping(i),signature,8) == 0){
				desc = ((acpi_rsdp_t*)Memory::GetIOMapping(i));

				goto success;
			}
		}

		{
		const char* panicReasons[]{"System not ACPI Complaiant."};
		KernelPanic(panicReasons,1);
		//return;
		}

		success:

		isos = new List<apic_iso_t*>();

		rsdtHeader = ((acpi_rsdt_t*)Memory::GetIOMapping(desc->rsdt));

		memcpy(oem,rsdtHeader->header.oem,6);
		oem[6] = 0; // Zero OEM String

		Log::Info("[ACPI] Revision: %d", rsdtHeader->header.revision);
		Log::Info("[ACPI] OEM ID: %s", oem);

		fadt = (acpi_fadt_t*)FindSDT("FACP", 0);

		asm("cli");

		lai_set_acpi_revision(rsdtHeader->header.revision);
		lai_create_namespace();

		ReadMADT();

		asm("sti");
	}

	void SetRSDP(acpi_rsdp_t* p){
		desc = p;
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
	[[noreturn]] void laihost_panic(const char *msg){
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
		uint64_t delayInTicks = (freq/1000)*ms;
		uint64_t seconds = Timer::GetSystemUptime();
		uint64_t ticks = Timer::GetTicks();
		uint64_t totalTicks = seconds*freq + ticks;

		for(;;){
			uint64_t totalTicksNew = Timer::GetSystemUptime()*freq + Timer::GetTicks();
			if(totalTicksNew - totalTicks == delayInTicks){
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

}*/

/* Read a byte/word/dword from the given device's PCI configuration space
   at the given offset. * /
uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset){

}

uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset){

}

uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset){

}*/

}