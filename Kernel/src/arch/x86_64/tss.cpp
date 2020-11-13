#include <tss.h>

#include <string.h>
#include <paging.h>
#include <physicalallocator.h>

extern "C"
void LoadTSS(uint64_t address, uint64_t gdt, uint8_t selector);

namespace TSS
{
    void InitializeTSS(tss_t* tss, void* gdt){

        LoadTSS((uintptr_t)tss, (uint64_t)gdt, 0x28);

        memset(tss, 0, sizeof(tss_t));
        
        // Set up Interrupt Stack Tables
        tss->ist1 = (uint64_t)Memory::KernelAllocate4KPages(1);
        tss->ist2 = (uint64_t)Memory::KernelAllocate4KPages(1);
        tss->ist3 = (uint64_t)Memory::KernelAllocate4KPages(1);

        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), tss->ist1, 1);
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), tss->ist2, 1);
        Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), tss->ist3, 1);

        memset((void*)tss->ist1, 0, PAGE_SIZE_4K);
        memset((void*)tss->ist2, 0, PAGE_SIZE_4K);
        memset((void*)tss->ist3, 0, PAGE_SIZE_4K);

        asm volatile("mov %%rsp, %0" : "=r"(tss->rsp0));
            
        asm volatile("ltr %%ax" :: "a"(0x2B));
    }
}