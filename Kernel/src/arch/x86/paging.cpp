#include <paging.h>
#include <idt.h>
//#include <memory.h>
//#include <fatal.h>
#include <string.h>
#include <logging.h>
#include <system.h>
#include <scheduler.h>
#include <physicalallocator.h>
#include <panic.h>

//extern uint32_t kernel_end;

namespace Memory{
	uint32_t maxPages = 1024 * 1023;

	page_directory_t kernelPageDirectory __attribute__((aligned(4096)));
	page_table_t kernelPageTables[TABLES_PER_DIR] __attribute__((aligned(4096)));

	page_directory_ptr_t kernelPageDirectoryPtr;
	

	page_directory_t* currentPageDirectory;
	page_table_t* currentPageTables;

	static page_t* get_page(uint32_t addr)
	{
		uint32_t pdindex = PAGE_DIRECTORY_INDEX(addr);
		uint32_t ptindex = PAGE_TABLE_INDEX(addr);

		page_table_t* pt = &(currentPageTables[pdindex]);
		return &(*pt)[ptindex];
	}

	/*uint32_t AllocateVirtualPages(uint32_t amount, page_directory_ptr_t dir) {
		uint32_t page_dir_index = 0;
		uint32_t address = 0;
		uint32_t offset = 0;
		uint32_t counter = 0;

		amount += 2;

		for(uint32_t i = page_dir_index; i < PAGE_DIRECTORY_INDEX(0xC0000000); i++){
			for(uint32_t j = 0; j < PAGES_PER_TABLE; j++){
				if(dir.page_tables[i][j] & PAGE_PRESENT){
					offset = j;
					page_dir_index = i;
					counter = 0;
					continue;
				}

				counter++;

				if(counter == amount){
					address = (page_dir_index * PAGES_PER_TABLE + offset) * PAGE_SIZE + PAGE_SIZE;
					while(counter){
						SetFlags(&(dir.page_tables[page_dir_index][offset]), PAGE_WRITABLE | PAGE_PRESENT);
						SetFlags(&((*dir.page_directory)[page_dir_index]),PDE_PRESENT | PDE_WRITABLE);
						SetPDEFrame(&((*dir.page_directory)[page_dir_index]),(uint32_t)&(dir.page_tables[page_dir_index]) - 0xC0000000);
						offset++;
						counter--;

						if(offset >= PAGES_PER_TABLE){
							page_dir_index++;
							offset = 0;
						} 
					}

					return address;
				}
			}
		}

		//write_serial_string("No more pages availiable!\n");
		return 0;
	}*/

	uint32_t AllocateVirtualPages(uint32_t amount, page_directory_ptr_t dir) { // Used for kernel heap
		uint32_t page_dir_index = PAGE_DIRECTORY_INDEX(0x40000);
		uint32_t address = 0;
		uint32_t offset = PAGE_TABLE_INDEX(0x40000);
		uint32_t counter = 0;

		amount += 2;

		for(uint32_t i = page_dir_index; i < TABLES_PER_DIR; i++){
			for(uint32_t j = 0; j < PAGES_PER_TABLE; j++){
				if(dir.page_tables[i][j] & PAGE_PRESENT){
					offset = j;
					page_dir_index = i;
					counter = 0;
					continue;
				}

				counter++;

				if(counter >= amount){
					address = (page_dir_index * PAGES_PER_TABLE + offset) * PAGE_SIZE + PAGE_SIZE;
					while(counter){
						SetFlags(&(dir.page_tables[page_dir_index][offset]), PAGE_WRITABLE | PAGE_PRESENT);
						SetFlags(&((*dir.page_directory)[page_dir_index]),PDE_PRESENT | PDE_WRITABLE);
						SetPDEFrame(&((*dir.page_directory)[page_dir_index]),(uint32_t)&(dir.page_tables[page_dir_index]) - 0xC0000000);
						offset++;
						counter--;

						if(offset >= PAGES_PER_TABLE){
							page_dir_index++;
							offset = 0;
						} 
					}
					return address;
				}
			}
		}
		
		Log::Error("No more pages for kernel heap availiable!\n");
		//fatal_error("The kernel has run out of virtual memory for its heap", "KERNEL_HEAP_FULL");
		for(;;);
		//return 0;
	}

	uint32_t KernelAllocateVirtualPages(uint32_t amount) { // Used for kernel heap
		uint32_t page_dir_index = PAGE_DIRECTORY_INDEX(0xC0000000);
		uint32_t address = 0;
		uint32_t offset = 0;
		uint32_t counter = 0;

		amount += 1;

		for(uint32_t i = page_dir_index; i < TABLES_PER_DIR; i++){
			for(uint32_t j = 0; j < PAGES_PER_TABLE; j++){
				if(kernelPageTables[i][j] & PAGE_PRESENT){
					offset = j;
					page_dir_index = i;
					counter = 0;
					continue;
				}

				counter++;

				if(counter >= amount){
					address = (page_dir_index * PAGES_PER_TABLE + offset) * PAGE_SIZE + PAGE_SIZE;
					while(counter){
						SetFlags(&(kernelPageTables[page_dir_index][offset]), PAGE_WRITABLE | PAGE_PRESENT);
						SetFlags(&(kernelPageDirectory[page_dir_index]),PDE_PRESENT | PDE_WRITABLE);
						SetPDEFrame(&(kernelPageDirectory[page_dir_index]),(uint32_t)&(kernelPageTables[page_dir_index]) - 0xC0000000);
						offset++;
						counter--;

						if(offset >= PAGES_PER_TABLE){
							page_dir_index++;
							offset = 0;
						} 
					}

					return address;
				}
			}
		}
		
		Log::Error("No more pages for kernel heap availiable!\n");
		//fatal_error("The kernel has run out of virtual memory for its heap", "KERNEL_HEAP_FULL");
		for(;;);
		//return 0;
	}


	uint32_t VirtualToPhysicalAddress(uint32_t addr) {
		//return ((uint32_t*)(*currentPageDirectory[addr >> 22] & ~0xfff))[addr << 10 >> 10 >> 12];
		uint32_t phys_addr;

		uint32_t pd_index = PAGE_DIRECTORY_INDEX(addr);
		uint32_t pt_index = PAGE_TABLE_INDEX(addr);
		
		phys_addr = (GetPageFrame(currentPageTables[pd_index][pt_index])) << 12;
		return phys_addr;
	}

	void FreeVirtualPages(uint32_t virt, uint32_t amount)
	{
		for (uint32_t i = 0; i < amount; i++) {
			page_t* pg = get_page(virt + i*PAGE_SIZE);
			ClearFlags(pg, PAGE_PRESENT | PAGE_WRITABLE);
		}
	}

	// Map amount pages from phys to virt
	bool MapVirtualPages(uint32_t phys, uint32_t virt, uint32_t amount) {
		if (!amount) {
			//write_serial_string("Error Mapping Address: Page Amount is invalid or zero (MapVirtualPages)");
			return 0;
		}

		for (uint32_t i = 0; i < amount; i++, phys += PAGE_SIZE, virt += PAGE_SIZE) {

			/*//write_serial_string("\r\nMapping 0x");
			//write_serial_string(itoa(virt,(char*)0xC0000000,16));
			//write_serial_string(" to 0x");
			//write_serial_string(itoa(phys,(char*)0xC0000000,16));*/

			uint32_t pdindex = PAGE_DIRECTORY_INDEX(virt);

			// Get the page from the page table
			page_t* page = get_page(virt);
			SetFlags(page, PAGE_PRESENT | PAGE_WRITABLE); // Make it present and writeable
			SetPageFrame(page, phys); // Point it to the physical address

			// Add it to the page table and directory
			SetFlags(&((*currentPageDirectory)[pdindex]), PDE_PRESENT | PDE_WRITABLE);
			SetPDEFrame(&((*currentPageDirectory)[pdindex]), (uint32_t)&(currentPageTables[pdindex]) - 0xC0000000);
		}
		return 1;
	}

	// Map amount pages from phys to virt
	bool MapVirtualPages(uint32_t phys, uint32_t virt, uint32_t amount, page_directory_ptr_t pdir) {
		if (!amount) {
			//write_serial_string("Error Mapping Address: Page Amount is invalid or zero (MapVirtualPages)");
			return 0;
		}

		for (uint32_t i = 0; i < amount; i++, phys += PAGE_SIZE, virt += PAGE_SIZE) {
			uint32_t pdindex = PAGE_DIRECTORY_INDEX(virt);

			page_table_t* old_page_tables = currentPageTables;

			// Get the page from the page table
			currentPageTables = pdir.page_tables;
			page_t* page = get_page(virt);
			currentPageTables = old_page_tables;
			SetFlags(page, PAGE_PRESENT | PAGE_WRITABLE); // Make it present and writeable
			SetPageFrame(page, phys); // Point it to the physical address

			// Add it to the page table and directory
			SetFlags(&((*pdir.page_directory)[pdindex]), PDE_PRESENT | PDE_WRITABLE);
			SetPDEFrame(&((*pdir.page_directory)[pdindex]), (uint32_t)VirtualToPhysicalAddress((uint32_t)&(pdir.page_tables[pdindex])));
		}
		return 1;
	}

	void InitializeVirtualMemory()
	{
		IDT::RegisterInterruptHandler(14,PageFaultHandler);

		currentPageDirectory = &kernelPageDirectory;
		currentPageTables = kernelPageTables;

		for (uint32_t i = 0; i < TABLES_PER_DIR; i++) {
			for (uint32_t j = 0; j < PAGES_PER_TABLE; j++) {
				kernelPageTables[i][j] = 0;
			}
			kernelPageDirectory[i] = 0 | PDE_WRITABLE;
		}

		for (int i = 0, frame = 0x0, virt = 0; i<4096; i++, frame += PAGE_SIZE, virt += PAGE_SIZE) {

			//Create a new page
			page_t page = 0;
			SetFlags(&page, PAGE_PRESENT | PAGE_WRITABLE); // Make it present and writable
			SetPageFrame(&page, frame);

			// Add it to the page table and directory
			uint32_t pdindex = PAGE_DIRECTORY_INDEX(virt);
			SetFlags(&kernelPageDirectory[pdindex], PDE_PRESENT);
			SetFlags(&kernelPageDirectory[pdindex], PDE_WRITABLE);
			SetPDEFrame(&(kernelPageDirectory[pdindex]), ((uint32_t)&(kernelPageTables[pdindex])) - 0xC0000000);
			kernelPageTables[pdindex][PAGE_TABLE_INDEX(virt)] = page;
		}

		for (int i = 0, frame = 0x0, virt = 0xC0000000; i<4096; i++, frame += PAGE_SIZE, virt += PAGE_SIZE) {

			//Create a new page
			page_t page = 0;
			SetFlags(&page, PAGE_PRESENT); // Make it present and writable
			SetFlags(&page, PAGE_WRITABLE); // Make it present and writable
			SetPageFrame(&page, frame);

			// Add it to the page table and directory
			uint32_t pdindex = PAGE_DIRECTORY_INDEX(virt);
			SetFlags(&kernelPageDirectory[pdindex], PDE_PRESENT);
			SetFlags(&kernelPageDirectory[pdindex], PDE_WRITABLE);
			SetPDEFrame(&(kernelPageDirectory[pdindex]), ((uint32_t)(&(kernelPageTables[pdindex]))) - 0xC0000000);
			kernelPageTables[pdindex][PAGE_TABLE_INDEX(virt)] = page;
		}

		SwitchPageDirectory((uint32_t)kernelPageDirectory - 0xC0000000);
	}

	void LoadKernelPageDirectory(){
		SwitchPageDirectory((uint32_t)kernelPageDirectory - 0xC0000000);
	}

	uint32_t GetKernelPageDirectory(){
		return (uint32_t)kernelPageDirectory - 0xC0000000;
	}

	// Switch Page Directory
	void SwitchPageDirectory(uint32_t dir)
	{
		asm volatile("mov %0, %%cr3":: "r"(dir));
	}

	// Sets the current page directory but does not switch
	void SetCurrentPageDirectory(page_directory_ptr_t dir) {
		currentPageDirectory = dir.page_directory;
		currentPageTables = dir.page_tables;
	}

	// Sets the current page directory to that of the kernel but does not switch
	void SetCurrentPageDirectoryToKernel() {
		currentPageDirectory = &kernelPageDirectory;
		currentPageTables = kernelPageTables;
	}

	page_directory_ptr_t CreateAddressSpace() {
		asm("cli");
		page_directory_ptr_t ptr;

		ptr.page_directory = (page_directory_t*)KernelAllocateVirtualPages(1); // Allocate space for the page directory
		ptr.page_directory_phys = AllocatePhysicalMemoryBlock();

		MapVirtualPages((uint32_t)ptr.page_directory_phys, (uint32_t)ptr.page_directory, 1); // Map page directory into kernel space
		memset(ptr.page_directory,0,sizeof(page_directory_t));

		for(uint32_t i = PAGE_DIRECTORY_INDEX(0xC0000000); i < TABLES_PER_DIR; i++){
			SetFlags(&((*ptr.page_directory)[i]), PDE_PRESENT | PDE_WRITABLE); // Map everything above the 3GB mark (kernel space)
			SetPDEFrame(&((*ptr.page_directory)[i]), (uint32_t)&(kernelPageTables[i]) - 0xC0000000);
		}

		ptr.page_tables = (page_table_t*)KernelAllocateVirtualPages((sizeof(page_table_t) * PAGE_DIRECTORY_INDEX(0xC0000000) + 1) / PAGE_SIZE); // Don't allocate space for page tables above 3 GB in memory

		for(uint32_t i = 0; i < PAGE_DIRECTORY_INDEX(0xC0000000); i++){
			uint32_t phys = AllocatePhysicalMemoryBlock();
			MapVirtualPages(phys,(uint32_t)&(ptr.page_tables[i]), 1); // 1 page table takes up 4096 bytes (1 page) in memory
			//MapVirtualPages(phys,((uint32_t)ptr.page_tables) + i * PAGE_SIZE, 1);
		}

		for(uint32_t i = 0; i < PAGE_DIRECTORY_INDEX(0xC0000000); i++){
			SetFlags(&((*ptr.page_directory)[i]), PDE_PRESENT | PDE_WRITABLE | PDE_USER);
			//SetPDEFrame(&((*ptr.page_directory)[i]), VirtualToPhysicalAddress((uint32_t)&(ptr.page_tables[i])));
		}

		memset((uint8_t*)ptr.page_tables, 0, sizeof(page_table_t) * PAGE_DIRECTORY_INDEX(0xC0000000));

		asm("sti");
		return ptr;
	}

	void PageFaultHandler(regs32_t* regs)
	{
		asm("cli");
		Log::Error("Page Fault!\r\n");
		Log::SetVideoConsole(nullptr);

		uint32_t faultAddress;
		asm volatile("mov %%cr2, %0" : "=r" (faultAddress));

		int present = !(regs->err_code & 0x1); // Page not present
		int rw = regs->err_code & 0x2;           // Attempted write to read only page
		int us = regs->err_code & 0x4;           // Processor was in user-mode and tried to access kernel page
		int reserved = regs->err_code & 0x8;     // Overwritten CPU-reserved bits of page entry
		int id = regs->err_code & 0x10;          // Caused by an instruction fetch

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

		
		//Log::Info(reason); // Print fault to serial

		Log::Info("EIP:");

		Log::Info("Process:");
		Log::Info(Scheduler::GetCurrentProcess()->pid);

		Log::Info(regs->eip);

		Log::Info("\r\nFault address: ");
		Log::Info(faultAddress);

		char temp[16];
		char temp2[16];
		char temp3[16];
		char* reasons[]{"Page Fault","EIP: ", itoa(regs->eip, temp, 16),"Address: ",itoa(faultAddress, temp2, 16), "Process:", itoa(Scheduler::GetCurrentProcess()->pid,temp3,10)};;
		KernelPanic(reasons,7);

		for (;;);
	}
}