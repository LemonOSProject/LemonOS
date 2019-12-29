
#include <paging.h>
#include <physicalallocator.h>
#include <serial.h>
#include <string.h>
#include <stddef.h>
	
extern "C" {
int liballoc_lock() {
	asm("cli");
	return 0;
}

int liballoc_unlock() {
	asm("sti");
	return 0;
}

#ifdef Lemon32


	void* liballoc_alloc(int pages) {
		void* addr = (void*)Memory::KernelAllocateVirtualPages(pages);
		for (int i = 0; i < pages; i++)
		{
			uint32_t phys = Memory::AllocatePhysicalMemoryBlock();
			Memory::MapVirtualPages(phys, (uint32_t)addr + i * PAGE_SIZE, 1);
		}
		return addr;
	}

	int liballoc_free(void* addr, size_t pages) {
		Memory::FreeVirtualPages((uint32_t)addr, pages);
		return 0;
	}
#endif

#ifdef Lemon64
void* liballoc_alloc(int pages) {
	void* addr = (void*)Memory::KernelAllocate4KPages(pages);
	for (int i = 0; i < pages; i++)
	{
		uint64_t phys = Memory::AllocatePhysicalMemoryBlock();
		Memory::KernelMapVirtualMemory4K(phys, (uint64_t)addr + i * PAGE_SIZE_4K, 1);
	}
	return addr;
}

int liballoc_free(void* addr, size_t pages) {
	#warning "liballoc_free is a stub"
	//Memory::KernelFree4KPages(addr, pages);
	return 0;
}
#endif
}