
#include <paging.h>
#include <physicalallocator.h>
#include <serial.h>
#include <string.h>
#include <stddef.h>
#include <lock.h>
#include <logging.h>
#include <scheduler.h>
#include <cpu.h>
	
volatile int liballocLock = 0;

extern "C" {
	
int liballoc_lock() {
	while(acquireTestLock(&liballocLock)) {
		assert(CheckInterrupts());
	} // If for some reason liballoc is locked before the scheduler starts something probably went horribly wrong
	return 0;
}

int liballoc_unlock() {
	releaseLock(&liballocLock);
	return 0;
}

void liballoc_kprintf(const char* __restrict fmt, ...){
	va_list args;
	va_start(args, fmt);
	Log::WriteF(fmt, args);
	va_end(args);
}

void* liballoc_alloc(size_t pages) {
	void* addr = (void*)Memory::KernelAllocate4KPages(pages);
	for (size_t i = 0; i < pages; i++)
	{
		uint64_t phys = Memory::AllocatePhysicalMemoryBlock();
		Memory::KernelMapVirtualMemory4K(phys, (uint64_t)addr + i * PAGE_SIZE_4K, 1);
	}

	memset(addr, 0, pages * PAGE_SIZE_4K);

	return addr;
}

int liballoc_free(void* addr, size_t pages) {
	for(size_t i = 0; i < pages; i++){
		uint64_t phys = Memory::VirtualToPhysicalAddress((uintptr_t)addr + i * PAGE_SIZE_4K);
		Memory::FreePhysicalMemoryBlock(phys);
	}
	Memory::KernelFree4KPages(addr, pages);
	return 0;
}

}