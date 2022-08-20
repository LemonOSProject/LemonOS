#include <TSS.h>

#include <CString.h>
#include <Paging.h>
#include <PhysicalAllocator.h>

extern "C" void LoadTSS(uint64_t address, uint64_t gdt, uint8_t selector);

namespace TSS {
void InitializeTSS(tss_t* tss, void* gdt) {

    LoadTSS((uintptr_t)tss, (uint64_t)gdt, 0x28);

    memset(tss, 0, sizeof(tss_t));

    // Set up Interrupt Stack Tables
    tss->ist1 = (uint64_t)Memory::KernelAllocate4KPages(8);
    tss->ist2 = (uint64_t)Memory::KernelAllocate4KPages(8);
    tss->ist3 = (uint64_t)Memory::KernelAllocate4KPages(8);

    for (unsigned i = 0; i < 8; i++) {
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), tss->ist1 + 8 * PAGE_SIZE_4K, 1);
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), tss->ist2 + 8 * PAGE_SIZE_4K, 1);
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), tss->ist3 + 8 * PAGE_SIZE_4K, 1);
    }

    memset((void*)tss->ist1, 0, PAGE_SIZE_4K);
    memset((void*)tss->ist2, 0, PAGE_SIZE_4K);
    memset((void*)tss->ist3, 0, PAGE_SIZE_4K);

    tss->ist1 += PAGE_SIZE_4K * 8;
    tss->ist2 += PAGE_SIZE_4K * 8;
    tss->ist3 += PAGE_SIZE_4K * 8;

    asm volatile("mov %%rsp, %0" : "=r"(tss->rsp0));

    asm volatile("ltr %%ax" ::"a"(0x2B));
}
} // namespace TSS
