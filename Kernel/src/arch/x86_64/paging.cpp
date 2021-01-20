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
#include <apic.h>
#include <strace.h>

//extern uint32_t kernel_end;

#define KERNEL_HEAP_PDPT_INDEX 511
#define KERNEL_HEAP_PML4_INDEX 511

address_space_t* currentAddressSpace;

uint64_t kernelPML4Phys;
extern int lastSyscall;

namespace Memory{
	pml4_t kernelPML4 __attribute__((aligned(4096)));
	pdpt_t kernelPDPT __attribute__((aligned(4096))); // Kernel itself will reside here (0xFFFFFFFF80000000)
	page_dir_t kernelDir __attribute__((aligned(4096)));
	page_dir_t kernelHeapDir __attribute__((aligned(4096)));
	page_t kernelHeapDirTables[TABLES_PER_DIR][PAGES_PER_TABLE] __attribute__((aligned(4096)));
	page_dir_t ioDirs[4] __attribute__((aligned(4096)));

	uint64_t VirtualToPhysicalAddress(uint64_t addr) {
		uint64_t address = 0;

		uint32_t pml4Index = PML4_GET_INDEX(addr);
		uint32_t pageDirIndex = PAGE_DIR_GET_INDEX(addr);
		uint32_t pageTableIndex = PAGE_TABLE_GET_INDEX(addr);

		if(pml4Index < 511){ // From Process Address Space
			
		} else { // From Kernel Address Space
			if(kernelHeapDir[pageDirIndex] & 0x80){
				address = (GetPageFrame(kernelHeapDir[pageDirIndex])) << 12;
			} else {
				address = (GetPageFrame(kernelHeapDirTables[pageDirIndex][pageTableIndex])) << 12;
			}
		}
		return address;
	}

	uint64_t VirtualToPhysicalAddress(uint64_t addr, address_space_t* addressSpace) {
		uint64_t address = 0;

		uint32_t pml4Index = PML4_GET_INDEX(addr);
		uint32_t pdptIndex = PDPT_GET_INDEX(addr);
		uint32_t pageDirIndex = PAGE_DIR_GET_INDEX(addr);
		uint32_t pageTableIndex = PAGE_TABLE_GET_INDEX(addr);

		if(pml4Index == 0){ // From Process Address Space
			if((addressSpace->pageDirs[pdptIndex][pageDirIndex] & 0x1) && addressSpace->pageTables[pdptIndex][pageDirIndex])
				return addressSpace->pageTables[pdptIndex][pageDirIndex][pageTableIndex] & PAGE_FRAME;
			else return 0;		
		} else { // From Kernel Address Space
			if(kernelHeapDir[pageDirIndex] & 0x80){
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
		
		SetPageFrame(&(kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)]),((uint64_t)kernelPDPT - KERNEL_VIRTUAL_BASE));
		kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)] |= 0x3;
		kernelPML4[0] = kernelPML4[PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)];

		kernelPDPT[PDPT_GET_INDEX(KERNEL_VIRTUAL_BASE)] = ((uint64_t)kernelDir - KERNEL_VIRTUAL_BASE) | 0x3;
		for(int j = 0; j < TABLES_PER_DIR; j++){
			kernelDir[j] = (PAGE_SIZE_2M * j) | 0x83;
		}

		kernelPDPT[KERNEL_HEAP_PDPT_INDEX] = 0x3;
		SetPageFrame(&(kernelPDPT[KERNEL_HEAP_PDPT_INDEX]), (uint64_t)kernelHeapDir - KERNEL_VIRTUAL_BASE);

		for(int i = 0; i < 4; i++){
			kernelPDPT[PDPT_GET_INDEX(IO_VIRTUAL_BASE) + i] = ((uint64_t)ioDirs[i] - KERNEL_VIRTUAL_BASE) | 0x3;//(PAGE_SIZE_1G * i) | 0x83;
			for(int j = 0; j < TABLES_PER_DIR; j++){
				ioDirs[i][j] = (PAGE_SIZE_1G * i + PAGE_SIZE_2M * j) | (PDE_2M | PDE_WRITABLE | PDE_PRESENT | PDE_CACHE_DISABLED);
			}
		}
		
		kernelPDPT[0] = kernelPDPT[PDPT_GET_INDEX(KERNEL_VIRTUAL_BASE)]; // Its important that we identity map low memory for SMP

		for(int i = 0; i < TABLES_PER_DIR; i++){
			memset(&(kernelHeapDirTables[i]),0,sizeof(page_t)*PAGES_PER_TABLE);
		}
		
		kernelPML4Phys = (uint64_t)kernelPML4 - KERNEL_VIRTUAL_BASE;
		asm("mov %%rax, %%cr3" :: "a"((uint64_t)kernelPML4 - KERNEL_VIRTUAL_BASE));
	}

	address_space_t* CreateAddressSpace(){
		address_space_t* addressSpace = (address_space_t*)kmalloc(sizeof(address_space_t));
		
		pdpt_entry_t* pdpt = (pdpt_entry_t*)Memory::KernelAllocate4KPages(1); // PDPT;
		uintptr_t pdptPhys = Memory::AllocatePhysicalMemoryBlock();
		Memory::KernelMapVirtualMemory4K(pdptPhys, (uintptr_t)pdpt,1);
		memset((pdpt_entry_t*)pdpt,0,4096);

		pd_entry_t** pageDirs = (pd_entry_t**)KernelAllocate4KPages(1); // Page Dirs
		Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)pageDirs,1);
		uint64_t* pageDirsPhys = (uint64_t*)KernelAllocate4KPages(1); // Page Dirs
		Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)pageDirsPhys,1);
		page_t*** pageTables = (page_t***)KernelAllocate4KPages(1); // Page Tables
		Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), (uintptr_t)pageTables,1);

		pml4_entry_t* pml4 = (pml4_entry_t*)KernelAllocate4KPages(1); // Page Tables
		uintptr_t pml4Phys = Memory::AllocatePhysicalMemoryBlock();
		Memory::KernelMapVirtualMemory4K(pml4Phys, (uintptr_t)pml4,1);
		memcpy(pml4, kernelPML4, 4096);

		for(int i = 0; i < 512; i++){
			pageDirs[i] = (pd_entry_t*)KernelAllocate4KPages(1);
			pageDirsPhys[i] = Memory::AllocatePhysicalMemoryBlock();
			KernelMapVirtualMemory4K(pageDirsPhys[i],(uintptr_t)pageDirs[i],1);

			pageTables[i] = (page_t**)Memory::KernelAllocate4KPages(1);
			KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),(uintptr_t)pageTables[i],1);

			SetPageFrame(&(pdpt[i]),pageDirsPhys[i]);
			pdpt[i] |= PDPT_WRITABLE | PDPT_PRESENT | PDPT_USER;

			memset(pageDirs[i],0,4096);
			memset(pageTables[i],0,4096);
		}

		addressSpace->pageDirs = pageDirs;
		addressSpace->pageDirsPhys = pageDirsPhys;
		addressSpace->pageTables = pageTables;
		addressSpace->pml4 = pml4;
		addressSpace->pdptPhys = pdptPhys;
		addressSpace->pml4Phys = pml4Phys;
		addressSpace->pdpt = pdpt;

		pml4[0] = pdptPhys | PML4_PRESENT | PML4_WRITABLE | PAGE_USER;

		return addressSpace;
	}

	void DestroyAddressSpace(address_space_t* addressSpace){
		for(int i = 0; i < DIRS_PER_PDPT; i++){
			for(int j = 0; j < TABLES_PER_DIR; j++){
				pd_entry_t dirEnt = addressSpace->pageDirs[i][j];
				if(dirEnt & PAGE_PRESENT){
					uint64_t phys = GetPageFrame(dirEnt);

					for(int k = 0; k < PAGES_PER_TABLE; k++){
						if(addressSpace->pageTables[i][j][k] & 0x1){
							uint64_t pagePhys = GetPageFrame(addressSpace->pageTables[i][j][k]);
							FreePhysicalMemoryBlock(pagePhys);
						}
					}

					FreePhysicalMemoryBlock(phys);
					addressSpace->pageDirs[i][j] = 0;
					KernelFree4KPages(addressSpace->pageTables[i][j], 1);
				}
				addressSpace->pageDirs[i][j] = 0;
			}

			addressSpace->pdpt[i] = 0;
			Memory::FreePhysicalMemoryBlock(addressSpace->pageDirsPhys[i]);
			KernelFree4KPages(addressSpace->pageDirs[i], 1);
		}
	}

	bool CheckRegion(uintptr_t addr, uint64_t len, address_space_t* addressSpace){
		return addr < PDPT_SIZE && (addr + len) < PDPT_SIZE && (addressSpace->pdpt[PDPT_GET_INDEX(addr)] & PDPT_USER) && (addressSpace->pdpt[PDPT_GET_INDEX(addr + len)] & PDPT_USER);
	}

	bool CheckKernelPointer(uintptr_t addr, uint64_t len){
		if(PML4_GET_INDEX(addr) != PML4_GET_INDEX(KERNEL_VIRTUAL_BASE)){
			return 0;
		}

		if(!(kernelPDPT[PDPT_GET_INDEX(addr)] & 0x1)){
			return 0;
		}
		
		if(PDPT_GET_INDEX(addr) == KERNEL_HEAP_PDPT_INDEX){
			if(!(kernelHeapDir[PAGE_DIR_GET_INDEX(addr)] & 0x1)){
				return 0;
			}

			if(!(kernelHeapDir[PAGE_DIR_GET_INDEX(addr)] & PDE_2M)){
				if(!(kernelHeapDirTables[PAGE_DIR_GET_INDEX(addr)][PAGE_TABLE_GET_INDEX(addr)] & 0x1)){
					return 0;
				}
			}
		} else if(PDPT_GET_INDEX(addr) == PDPT_GET_INDEX(KERNEL_VIRTUAL_BASE)){
			if(!(kernelDir[PAGE_DIR_GET_INDEX(addr)] & 0x1)){
				return 0;
			}
		} else {
			return 0;
		}

		return 1;
	}

	bool CheckUsermodePointer(uintptr_t addr, uint64_t len, address_space_t* addressSpace){
		if(addr >> 48) return 0; // Non-canonical or higher-half address

		if(!(addressSpace->pageDirs[PDPT_GET_INDEX(addr)][PAGE_DIR_GET_INDEX(addr)] & (PAGE_PRESENT))){
			return 0;
		}
		
		if(!(addressSpace->pageDirs[PDPT_GET_INDEX(addr + len)][PAGE_DIR_GET_INDEX(addr + len)] & (PAGE_PRESENT))){
			return 0;
		}

		if(!((addressSpace->pageTables[PDPT_GET_INDEX(addr)][PAGE_DIR_GET_INDEX(addr)][PAGE_TABLE_GET_INDEX(addr)] & (PAGE_PRESENT)) && addressSpace->pageTables[PDPT_GET_INDEX(addr)][PAGE_DIR_GET_INDEX(addr)][PAGE_TABLE_GET_INDEX(addr)] & (PAGE_USER))){
			return 0;
		}
		
		if(!((addressSpace->pageTables[PDPT_GET_INDEX(addr + len)][PAGE_DIR_GET_INDEX(addr + len)][PAGE_TABLE_GET_INDEX(addr + len)] & (PAGE_PRESENT)) && addressSpace->pageTables[PDPT_GET_INDEX(addr + len)][PAGE_DIR_GET_INDEX(addr + len)][PAGE_TABLE_GET_INDEX(addr + len)] & (PAGE_USER))){
			return 0;
		}

		return 1;
	}

	page_table_t AllocatePageTable(){
		void* virt = KernelAllocate4KPages(1);
		uint64_t phys = Memory::AllocatePhysicalMemoryBlock();

		KernelMapVirtualMemory4K(phys,(uintptr_t)virt, 1);

		page_table_t pTable;
		pTable.phys = phys;
		pTable.virt = (page_t*)virt;

		for(int i = 0; i < PAGES_PER_TABLE; i++){
			((page_t*)virt)[i] = 0;
		}

		return pTable;
	}

	void CreatePageTable(uint16_t pdptIndex, uint16_t pageDirIndex, address_space_t* addressSpace){
		page_table_t pTable = AllocatePageTable();
		SetPageFrame(&(addressSpace->pageDirs[pdptIndex][pageDirIndex]),pTable.phys);
		addressSpace->pageDirs[pdptIndex][pageDirIndex] |= PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
		addressSpace->pageTables[pdptIndex][pageDirIndex] = pTable.virt;
	}

	void* Allocate4KPages(uint64_t amount, address_space_t* addressSpace){
		uint64_t offset = 0;
		uint64_t pageDirOffset = 0;
		uint64_t counter = 0;
		uintptr_t address = 0;

		uint64_t pml4Index = 0;
		for(int d = 0; d < 512; d++){
			uint64_t pdptIndex = d;
			if(!(addressSpace->pdpt[d] & 0x1)) break;
			/* Attempt 1: Already Allocated Page Tables*/
			for(int i = 0; i < TABLES_PER_DIR; i++){
				if(addressSpace->pageDirs[d][i] & 0x1 && !(addressSpace->pageDirs[d][i] & 0x80)){
					for(int j = 0; j < PAGES_PER_TABLE; j++){
						if(addressSpace->pageTables[d][i][j] & 0x1){
							pageDirOffset = i;
							offset = j+1;
							counter = 0;
							continue;
						}

						counter++;

						if(counter >= amount){
							address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
							while(counter--){
								if(offset >= 512){
									pageDirOffset++;
									offset = 0;
								}
								addressSpace->pageTables[d][pageDirOffset][offset] = 0x3;
								offset++;
							}

							return (void*)address;
						}
					}
				} else {
					pageDirOffset = i+1;
					offset = 0;
					counter = 0;
				}
			}
			
			pageDirOffset = 0;
			offset = 0;
			counter = 0;

			/* Attempt 2: Allocate Page Tables*/
			for(int i = 0; i < TABLES_PER_DIR; i++){
				if(!(addressSpace->pageDirs[d][i] & 0x1)){
					
					CreatePageTable(d,i,addressSpace);
					for(int j = 0; j < PAGES_PER_TABLE; j++){

						address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
						counter++;
						
						if(counter >= amount){
							address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
							while(counter--){
								if(offset >= 512){
									pageDirOffset ++;
									offset = 0;
								}
								addressSpace->pageTables[d][pageDirOffset][offset] = 0x3;
								offset++;
							}
							return (void*)address;
						}
					}
				} else {
					pageDirOffset = i+1;
					offset = 0;
					counter = 0;
				}
			}
			Log::Info("new dir");
		}

		const char* reasons[1] = {"Out of Virtual Memory!"};
		KernelPanic(reasons, 1);
	}

	void* KernelAllocate4KPages(uint64_t amount){
		uint64_t offset = 0;
		uint64_t pageDirOffset = 0;
		uint64_t counter = 0;
		uintptr_t address = 0;

		uint64_t pml4Index = KERNEL_HEAP_PML4_INDEX;
		uint64_t pdptIndex = KERNEL_HEAP_PDPT_INDEX;

		/* Attempt 1: Already Allocated Page Tables*/
		for(int i = 0; i < TABLES_PER_DIR; i++){
			if(kernelHeapDir[i] & 0x1 && !(kernelHeapDir[i] & 0x80)){
				for(int j = 0; j < TABLES_PER_DIR; j++){
					if(kernelHeapDirTables[i][j] & 0x1){
						pageDirOffset = i;
						offset = j+1;
						counter = 0;
						continue;
					}

					counter++;

					if(counter >= amount){
						address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
						address |= 0xFFFF000000000000;
						while(counter--){
							if(offset >= 512){
								pageDirOffset++;
								offset = 0;
							}
							kernelHeapDirTables[pageDirOffset][offset] = 0x3;
							offset++;
						}

						return (void*)address;
					}
				}
			} else {
				pageDirOffset = i+1;
				offset = 0;
				counter = 0;
			}
		}
		
		pageDirOffset = 0;
		offset = 0;
		counter = 0;

		/* Attempt 2: Allocate Page Tables*/
		for(int i = 0; i < TABLES_PER_DIR; i++){
			if(!(kernelHeapDir[i] & 0x1)){
				counter += 512;

				if(counter >= amount){
					address = (PDPT_SIZE * pml4Index) + (pdptIndex * PAGE_SIZE_1G) + (pageDirOffset * PAGE_SIZE_2M) + (offset*PAGE_SIZE_4K);
					address |= 0xFFFF000000000000;
					//kernelHeapDir[i] = (PAGE_FRAME & ((uintptr_t)&(kernelHeapDirTables[i]) - KERNEL_VIRTUAL_BASE)) | 0x3;
					SetPageFrame(&(kernelHeapDir[pageDirOffset]),((uintptr_t)&(kernelHeapDirTables[pageDirOffset]) - KERNEL_VIRTUAL_BASE));
					kernelHeapDir[pageDirOffset] |= 0x3;
					while(amount--){
						if(offset >= 512){
							pageDirOffset ++;
							offset = 0;	
							SetPageFrame(&(kernelHeapDir[pageDirOffset]),((uintptr_t)&(kernelHeapDirTables[pageDirOffset]) - KERNEL_VIRTUAL_BASE));
							kernelHeapDir[pageDirOffset] |= 0x3;
						}
						kernelHeapDirTables[pageDirOffset][offset] = 0x3;
						offset++;
					}
					return (void*)address;
				}
			} else {
				pageDirOffset = i+1;
				offset = 0;
				counter = 0;
			}
		}

		Log::Error("Kernel Out of Virtual Memory");
		const char* reasons[1] = {"Kernel Out of Virtual Memory!"};
		KernelPanic(reasons, 1);
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

		Log::Error("Kernel Out of Virtual Memory");
		const char* reasons[1] = {"Kernel Out of Virtual Memory!"};
		KernelPanic(reasons, 1);
		for(;;);
	}
	
	void KernelFree4KPages(void* addr, uint64_t amount){
		uint64_t pageDirIndex, pageIndex;
		uint64_t virt = (uint64_t)addr;

		while(amount--){
			pageDirIndex = PAGE_DIR_GET_INDEX(virt);
			pageIndex = PAGE_TABLE_GET_INDEX(virt);
			kernelHeapDirTables[pageDirIndex][pageIndex] = 0;
			invlpg(virt);
			virt += PAGE_SIZE_4K;
		}
	}

	void KernelFree2MPages(void* addr, uint64_t amount){
		while(amount--){
			uint64_t pageDirIndex = PAGE_DIR_GET_INDEX((uint64_t)addr);
			kernelHeapDir[pageDirIndex] = 0;
			addr = (void*)((uint64_t)addr + 0x200000);
		}
	}

	void Free4KPages(void* addr, uint64_t amount, address_space_t* addressSpace){
		uint64_t pml4Index, pdptIndex, pageDirIndex, pageIndex;

		uint64_t virt = (uint64_t)addr;

		while(amount--){
			pml4Index = PML4_GET_INDEX(virt);
			pdptIndex = PDPT_GET_INDEX(virt);
			pageDirIndex = PAGE_DIR_GET_INDEX(virt);
			pageIndex = PAGE_TABLE_GET_INDEX(virt);

			const char* panic[1] = {"Process address space cannot be >512GB"};
			if(pdptIndex > MAX_PDPT_INDEX || pml4Index) KernelPanic(panic,1);

			if(!(addressSpace->pageDirs[pdptIndex][pageDirIndex] & 0x1)) continue;
			
			addressSpace->pageTables[pdptIndex][pageDirIndex][pageIndex] = 0;

			invlpg(virt);

			virt += PAGE_SIZE_4K; /* Go to next page */
		}
	}

	void KernelMapVirtualMemory2M(uint64_t phys, uint64_t virt, uint64_t amount){
		uint64_t pageDirIndex = PAGE_DIR_GET_INDEX(virt);

		while(amount--){
			kernelHeapDir[pageDirIndex] = 0x83;
			SetPageFrame(&(kernelHeapDir[pageDirIndex]), phys);
			kernelHeapDir[pageDirIndex] |= 0x83;
			pageDirIndex++;
			phys += PAGE_SIZE_2M;
		}
	}

	void KernelMapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount, uint64_t flags){
		uint64_t pageDirIndex, pageIndex;

		while(amount--){
			pageDirIndex = PAGE_DIR_GET_INDEX(virt);
			pageIndex = PAGE_TABLE_GET_INDEX(virt);
			SetPageFrame(&(kernelHeapDirTables[pageDirIndex][pageIndex]), phys);
			kernelHeapDirTables[pageDirIndex][pageIndex] |= flags;
			invlpg(virt);
			phys += PAGE_SIZE_4K;
			virt += PAGE_SIZE_4K;
		}
	}

	void KernelMapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount){
		KernelMapVirtualMemory4K(phys, virt, amount, PAGE_WRITABLE | PAGE_PRESENT);
	}

	void MapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount, address_space_t* addressSpace){
		uint64_t pml4Index, pdptIndex, pageDirIndex, pageIndex;

		//phys &= ~(PAGE_SIZE_4K-1);
		//virt &= ~(PAGE_SIZE_4K-1);

		while(amount--){
			pml4Index = PML4_GET_INDEX(virt);
			pdptIndex = PDPT_GET_INDEX(virt);
			pageDirIndex = PAGE_DIR_GET_INDEX(virt);
			pageIndex = PAGE_TABLE_GET_INDEX(virt);

			const char* panic[1] = {"Process address space cannot be >512GB"};
			if(pdptIndex > MAX_PDPT_INDEX || pml4Index) KernelPanic(panic,1);

			if(!(addressSpace->pageDirs[pdptIndex][pageDirIndex] & 0x1)) CreatePageTable(pdptIndex,pageDirIndex,addressSpace); // If we don't have a page table at this address, create one.
			
			addressSpace->pageTables[pdptIndex][pageDirIndex][pageIndex] = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
			SetPageFrame(&(addressSpace->pageTables[pdptIndex][pageDirIndex][pageIndex]), phys);

			invlpg(virt);

			phys += PAGE_SIZE_4K;
			virt += PAGE_SIZE_4K; /* Go to next page */
		}
	}

	uintptr_t GetIOMapping(uintptr_t addr){
		if(addr > 0xffffffff){ // Typically most MMIO will not reside > 4GB, but check just in case
			Log::Error("MMIO >4GB current unsupported");
			return 0xffffffff;
		}

		return addr + IO_VIRTUAL_BASE;
	}

	void ChangeAddressSpace(address_space_t* addressSpace){
		currentAddressSpace = addressSpace;
	}

	void PageFaultHandler(void*, regs64_t* regs)
	{
		asm("cli");
		write_serial_n("Page Fault\r\n", 12);
		Log::SetVideoConsole(nullptr);

		int err_code = IDT::GetErrCode();

		uint64_t faultAddress;
		asm volatile("movq %%cr2, %0" : "=r" (faultAddress));

		int present = !(err_code & 0x1); // Page not present
		int rw = err_code & 0x2;           // Attempted write to read only page
		int us = err_code & 0x4;           // Processor was in user-mode and tried to access kernel page
		int reserved = err_code & 0x8;     // Overwritten CPU-reserved bits of page entry
		int id = err_code & 0x10;          // Caused by an instruction fetch

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

		Log::Info("RIP:");

		Log::Info(regs->rip);

		Log::Info("Process:");
		Log::Info(Scheduler::GetCurrentProcess()->pid);

		Log::Info("\r\nFault address: ");
		Log::Info(faultAddress);

		Log::Info("Register Dump: a: ");
		Log::Write(regs->rax);
		Log::Write(", b:");
		Log::Write(regs->rbx);
		Log::Write(", c:");
		Log::Write(regs->rcx);
		Log::Write(", d:");
		Log::Write(regs->rdx);
		Log::Write(", S:");
		Log::Write(regs->rsi);
		Log::Write(", D:");
		Log::Write(regs->rdi);
		Log::Write(", sp:");
		Log::Write(regs->rsp);
		Log::Write(", bp:");
		Log::Write(regs->rbp);

		if((regs->ss & 0x3)){
			Log::Warning("Process %s crashed, PID: ", Scheduler::GetCurrentProcess()->name);
			Log::Write(Scheduler::GetCurrentProcess()->pid);
			Log::Write(", RIP: ");
			Log::Write(regs->rip);
			Log::Info("Stack trace:");
			UserPrintStackTrace(regs->rbp, Scheduler::GetCurrentProcess()->addressSpace);
			Scheduler::EndProcess(Scheduler::GetCurrentProcess());
			return;
		};

		// Kernel Panic so tell other processors to stop executing
			APIC::Local::SendIPI(0, ICR_DSH_OTHER /* Send to all other processors except us */, ICR_MESSAGE_TYPE_FIXED, IPI_HALT);

		Log::Info("Last syscall: %d", lastSyscall);
			
		PrintStackTrace(regs->rbp);

		char temp[16];
		char temp2[16];
		char temp3[16];
		const char* reasons[]{"Page Fault","RIP: ", itoa(regs->rip, temp, 16),"Address: ",itoa(faultAddress, temp2, 16), "Process:", itoa(Scheduler::GetCurrentProcess()->pid,temp3,10)};;
		KernelPanic(reasons,7);
		for (;;);
	}
}