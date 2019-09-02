#include <paging.h>
#include <idt.h>
//#include <memory.h>
//#include <fatal.h>
#include <string.h>
#include <logging.h>
#include <system.h>
//#include <scheduler.h>
//#include <physicalallocator.h>
#include <panic.h>

//extern uint32_t kernel_end;

#define KERNEL_HEAP_PDPT_INDEX 511
#define KERNEL_HEAP_PML4_INDEX 511

extern uint64_t* kernel_pml4;
extern uint64_t* kernel_pdpt; // First 1GB Start Identity Mapped
extern uint64_t* kernel_pdpt2; // At entry to long mode, first 1GB is mapped to the last 2GB of virtual memory

namespace Memory{
	pml4_t kernelPML4 __attribute__((aligned(4096)));
	pdpt_t kernelPDPT __attribute__((aligned(4096))); // Kernel itself will reside here (0xFFFFFFFF80000000)
	//uint64_t* kernelHeapPDPT = kernel_pdpt; // We will reserve 1 GB of virtual memory for the kernel heap
	page_dir_t kernelHeapDir __attribute__((aligned(4096)));
	uint64_t* kernelHeapDirTables;

	uint32_t VirtualToPhysicalAddress(uint32_t addr) {
		//return ((uint32_t*)(*currentPageDirectory[addr >> 22] & ~0xfff))[addr << 10 >> 10 >> 12];
		uint32_t address;

		uint32_t pml4Index = PML4_GET_INDEX(addr);
		uint32_t pdptIndex = PDPT_GET_INDEX(addr);
		uint32_t pageDirIndex = PAGE_DIR_GET_INDEX(addr);

		if(kernelPML4[pdptIndex] & PDPT_1G){
			address = GetPageFrame(kernelPML4[pdptIndex]) << 12;
		}
		
		//address = (GetPageFrame(currentPageTables[pd_index][pt_index])) << 12;
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
		asm("mov %%rax, %%cr3" :: "a"((uint64_t)kernelPML4 - KERNEL_VIRTUAL_BASE));
	}

	void* KernelAllocate4KPages(uint64_t amount){
		for(int i = 0; i < TABLES_PER_DIR; i++){
			if(kernelHeapDir[i] & 0x3){
				/*for(int j = 0; j < TABLES_PER_DIR; i++){
					if([i][j] & 0x3){

					}
				}*/
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
				Log::Info("pdpt: ");
				Log::Info(pdptIndex, false);
				Log::Info("pml4: ");
				Log::Info(pml4Index, false);
				Log::Info("pdir: ");
				Log::Info(offset, false);

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

	void KernelMap2MPages(uint64_t phys, uint64_t virt, uint64_t amount){
		uint64_t pml4Index = PML4_GET_INDEX(virt);
		uint64_t pdptIndex = PDPT_GET_INDEX(virt);
		uint64_t pageDirIndex = PAGE_DIR_GET_INDEX(virt);
		//phys = 0;
				Log::Info("2pdpt: ");
				Log::Info(pdptIndex, false);
				Log::Info("2pml4: ");
				Log::Info(pml4Index, false);
				Log::Info("2pdir: ");
				Log::Info(pageDirIndex, false);

		while(amount--){
			Log::Info(phys);
			kernelHeapDir[pageDirIndex] = 0x83;
			SetPageFrame(&(kernelHeapDir[pageDirIndex]), phys);
			kernelHeapDir[pageDirIndex] |= 0x83;
			Log::Info(kernelHeapDir[pageDirIndex]);
			pageDirIndex++;
			phys += PAGE_SIZE_2M;
		}

		asm("invlpg (%%rbx)" :: "b"(virt));
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