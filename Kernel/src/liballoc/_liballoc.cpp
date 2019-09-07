
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
	void* addr = (void*)Memory::KernelAllocate2MPages(pages);
	for (int i = 0; i < pages; i++)
	{
		uint32_t phys = Memory::AllocateLargePhysicalMemoryBlock();
		Memory::KernelMapVirtualMemory2M(phys, (uint64_t)addr + i * 0x200000, 1);
	}
	return addr;
}

int liballoc_free(void* addr, size_t pages) {
	Memory::KernelFree2MPages(addr, pages);
	return 0;
}
#endif
}