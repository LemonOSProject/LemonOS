#include <paging.h>
#include <idt.h>
#include <memory.h>
#include <panic.h>
#include <string.h>
#include <logging.h>
#include <system.h>
#include <scheduler.h>
#include <physicalallocator.h>
#include <panic.h>

//extern uint32_t kernel_end;

#define KERNEL_HEAP_PDPT_INDEX 511
#define KERNEL_HEAP_PML4_INDEX 511

extern uint64_t* kernel_pml4;
extern uint64_t* kernel_pdpt; // First 1GB Start Identity Mapped
extern uint64_t* kernel_pdpt2; // At entry to long mode, first 1GB is mapped to the last 2GB of virtual memory

address_space_t* currentAddressSpace;

namespace Memory{
	pml4_t kernelPML4 __attribute__((aligned(4096)));
	pdpt_t kernelPDPT __attribute__((aligned(4096))); // Kernel itself will reside here (0xFFFFFFFF80000000)
	//uint64_t* kernelHeapPDPT = kernel_pdpt; // We will reserve 1 GB of virtual memory for the kernel heap
	page_dir_t kernelHeapDir __attribute__((aligned(4096)));
	page_t kernelHeapDirTables[TABLES_PER_DIR][PAGES_PER_TABLE];

	uint64_t VirtualToPhysicalAddress(uint64_t addr) {
		uint64_t address;

		uint32_t pml4Index = PML4_GET_INDEX(addr);
		uint32_t pdptIndex = PDPT_GET_INDEX(addr);
		uint32_t pageDirIndex = PAGE_DIR_GET_INDEX(addr);
		uint32_t pageTableIndex = PAGE_TABLE_GET_INDEX(addr);

		if(pml4Index == 0){ // From Process Address Space
			
		} else { // From Kernel Address Space
			if(kernelHeapDir[pageDirIndex] | 0x80){
				address = (GetPageFrame(kernelHeapDir[pageDirIndex])) << 12;
			} else {
				address = (GetPageFrame(kernelHeapDirTables[pageDirIndex][pageTableIndex])) << 12;
			}
		}
		return address;
	}

	void InitializeVirtualMemory()
	{
		IDT::RegisterInterruptHandler(14,PageFaultHandler);
		memset(kernelPML4, 0, sizeof(pml4_t));
		memset(kernelPDPT, 0, sizeof(pdpt_t));
		memset(kernelHeapDir, 0, sizeof(page_dir_t));
		/*for(int i = 0; i < 512; i++){
			kernelPML4[i] = 0x2;
			kernelPDPT[i] = 0x2;
			kernelHeapDir[i] = 0x2;
		}*/
		SetPageFrame(&(kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)]),((uint64_t)kernelPDPT - KERNEL_VIRTUAL_BASE));
		kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)] |= 0x3;
		Log::Info((kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)]), true);
		kernelPML4[0] = kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)];
		kernelPDPT[PDPT_GET_INDEX(KERNEL_VIRTUAL_BASE)] = 0x83;
		kernelPDPT[0] = 0x83;
		kernelPDPT[KERNEL_HEAP_PDPT_INDEX] = 0x3;
		SetPageFrame(&(kernelPDPT[KERNEL_HEAP_PDPT_INDEX]), (uint64_t)kernelHeapDir - KERNEL_VIRTUAL_BASE);
		kernelPDPT[KERNEL_HEAP_PDPT_INDEX] | 0x3;
		Log::Info(kernelPDPT[KERNEL_HEAP_PDPT_INDEX]);

		for(int i = 0; i < TABLES_PER_DIR; i++){
			memset(&(kernelHeapDirTables[i]),0,sizeof(page_t)*PAGES_PER_TABLE);
		}

		asm("mov %%rax, %%cr3" :: "a"((uint64_t)kernelPML4 - KERNEL_VIRTUAL_BASE));
		//kernelPML4[0] = 0;
	}

	address_space_t* CreateAddressSpace(){
		address_space_t* addressSpace = (address_space_t*)KernelAllocate4KPages(sizeof(address_space_t)/PAGE_SIZE_4K + 1);
		for(int i = 0; i < sizeof(address_space_t)/PAGE_SIZE_4K + 1; i++){
			uintptr_t phys = Memory::AllocatePhysicalMemoryBlock();
			Memory::KernelMapVirtualMemory4K(phys, (uintptr_t)(addressSpace + i*PAGE_SIZE_4K),1);
		}
	}

	void* KernelAllocate4KPages(uint64_t amount){
		uint64_t offset = 0;
		uint64_t counter = 0;
		uintptr_t address = 0;

		uint64_t pml4Index = KERNEL_HEAP_PML4_INDEX;
		uint64_t pdptIndex = KERNEL_HEAP_PDPT_INDEX;

		/* Attempt 1: Already Allocated Page Tables*/
		for(int i = 0; i < TABLES_PER_DIR; i++){
			if(kernelHeapDir[i] & 0x1 && !(kernelHeapDir[i] & 0x80)){
				offset = 0;
				for(int j = 0; j < TABLES_PER_DIR; j++){
					if(kernelHeapDirTables[i][j] & 0x1){
						offset = i+1;
						counter = 0;
						continue;
					}

					counter++;

					if(counter >= amount){
						address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (i * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
						address |= 0xFFFF000000000000;
						while(counter-- > 0){
							kernelHeapDirTables[i][offset] = 0x3;
							offset++;
						}

						return (void*)address;
					}
				}
			}
		}
		
offset = 0;

		/* Attempt 2: Allocate Page Tables*/
		for(int i = 0; i < TABLES_PER_DIR; i++){
			if(!kernelHeapDir[i]){
				for(int j = 0; j < TABLES_PER_DIR; j++){
					address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (i * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
					address |= 0xFFFF000000000000;
					//kernelHeapDir[i] = (PAGE_FRAME & ((uintptr_t)&(kernelHeapDirTables[i]) - KERNEL_VIRTUAL_BASE)) | 0x3;
					SetPageFrame(&(kernelHeapDir[i]),((uintptr_t)&(kernelHeapDirTables[i]) - KERNEL_VIRTUAL_BASE));
					kernelHeapDir[i] |= 0x3;
					while(counter--){
						kernelHeapDirTables[i][offset] = 0x3;
						offset++;
					}

					return (void*)address;
				}
			}
		}
	}

	void* KernelAllocate2MPages(uint64_t amount){
		uint64_t address = 0;
		uint64_t offset = 0;
		uint64_t counter = 0;
		uint64_t pdptIndex = KERNEL_HEAP_PDPT_INDEX;
		uint64_t pml4Index = KERNEL_HEAP_PML4_INDEX;
		
		for(int i = 0; i < TABLES_PER_DIR; i++){
			if(kernelHeapDir[i] & 0x1){
				offset = i+1;
				counter = 0;
				continue;
			}
			counter++;

			if(counter >= amount){
				address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + offset * PAGE_SIZE_2M;
				address |= 0xFFFF000000000000;
				while(counter--){
					kernelHeapDir[offset] = 0x83;
					offset++;
				}
				return (void*)address;
			}
		}

		*(int*)0xDEADC0DEDEADBEEF = 0xDEADBEEF;
		for(;;);
	}

	void KernelFree2MPages(void* addr, uint64_t amount){
		while(amount--){
			uint64_t pageDirIndex = PAGE_DIR_GET_INDEX((uint64_t)addr);
			kernelHeapDir[pageDirIndex] = 0;
			addr = (void*)((uint64_t)addr + 0x200000);
		}
	}

	void KernelMapVirtualMemory2M(uint64_t phys, uint64_t virt, uint64_t amount){
		uint64_t pml4Index = PML4_GET_INDEX(virt);
		uint64_t pdptIndex = PDPT_GET_INDEX(virt);
		uint64_t pageDirIndex = PAGE_DIR_GET_INDEX(virt);


		while(amount--){
			kernelHeapDir[pageDirIndex] = 0x83;
			SetPageFrame(&(kernelHeapDir[pageDirIndex]), phys);
			kernelHeapDir[pageDirIndex] |= 0x83;
			pageDirIndex++;
			phys += PAGE_SIZE_2M;
		}
	}

	void KernelMapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount){
		uint64_t pml4Index = PML4_GET_INDEX(virt);
		uint64_t pdptIndex = PDPT_GET_INDEX(virt);
		uint64_t pageDirIndex = PAGE_DIR_GET_INDEX(virt);
		uint64_t pageIndex = PAGE_TABLE_GET_INDEX(virt);

		while(amount--){
			pml4Index = PML4_GET_INDEX(virt);
			pdptIndex = PDPT_GET_INDEX(virt);
			pageDirIndex = PAGE_DIR_GET_INDEX(virt);
			pageIndex = PAGE_TABLE_GET_INDEX(virt);
			SetPageFrame(&(kernelHeapDirTables[pageDirIndex][pageIndex]), phys);
			kernelHeapDirTables[pageDirIndex][pageIndex] |= 0x83;
			phys += PAGE_SIZE_4K;
			virt += PAGE_SIZE_4K;
		}
	}

	void ChangeAddressSpace(address_space_t* addressSpace){
		currentAddressSpace = addressSpace;
		kernelPML4[0] = VirtualToPhysicalAddress((uint64_t)addressSpace->pdpt) | 0x3;
	}

	void PageFaultHandler(regs64_t* regs/*, int err_code*/)
	{
		asm("cli");
		Log::Error("Page Fault!\r\n");
		Log::SetVideoConsole(nullptr);

		uint64_t faultAddress;
		asm volatile("movq %%cr2, %0" : "=r" (faultAddress));

		/*int present = !(err_code & 0x1); // Page not present
		int rw = err_code & 0x2;           // Attempted write to read only page
		int us = err_code & 0x4;           // Processor was in user-mode and tried to access kernel page
		int reserved = err_code & 0x8;     // Overwritten CPU-reserved bits of page entry
		int id = err_code & 0x10;          // Caused by an instruction fetch

		char* msg = "Page fault ";
		char* reason;
		if (present)
			Log::Info("Page not present"); // Print fault to serial
		if (rw)
			Log::Info("Read Only");
		if (us)
			Log::Info("User mode process tried to access kernel memory");
		if (reserved)
			Log::Info("Reserved");
		if (id)
			Log::Info("instruction fetch");
*/
		
		//Log::Info(reason); // Print fault to serial

		Log::Info("EIP:");

		//Log::Info("Process:");
		//Log::Info(Scheduler::GetCurrentProcess()->pid);

		Log::Info(regs->rip);

		Log::Info("\r\nFault address: ");
		Log::Info(faultAddress);

		//char temp[16];
		//char temp2[16];
		//char temp3[16];
		//char* reasons[]{"Page Fault","EIP: ", itoa(regs->eip, temp, 16),"Address: ",itoa(faultAddress, temp2, 16), "Process:", itoa(Scheduler::GetCurrentProcess()->pid,temp3,10)};;
		//KernelPanic(reasons,7);

		for (;;);
	}
}