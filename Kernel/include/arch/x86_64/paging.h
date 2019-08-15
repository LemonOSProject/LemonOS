#include <stdint.h>

#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000

#define PML4_GET_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_GET_INDEX(addr) (((addr) >> 30) & 0x1FF)

#define PML4_PRESENT 1
#define PML4_WRITABLE (1 << 1)
#define PML4_FRAME 0xFFFFFFFFFF000

#define PDPT_PRESENT 1
#define PDPT_WRITABLE (1 << 1)
#define PDPT_1G (1 << 7)
#define PDPT_FRAME 0xFFFFFFFFFF000

#define PDE_PRESENT 1
#define PDE_WRITABLE (1 << 1)
#define PDE_2M (1 << 7)
#define PDE_FRAME 0xFFFFFFFFFF000

#define PAGE_PRESENT 1
#define PAGE_WRITABLE (1 << 1)
#define PAGE_FRAME 0xFFFFFFFFFF000

#define PAGE_SIZE_4K 4096
#define PAGE_SIZE_2M 0x200000
#define PAGE_SIZE_1G 0x40000000

namespace Memory{

    void InitializePaging();

    void* AllocateVirtualMemory(uint64_t size);
    void* KernelAllocateVirtualMemory(uint64_t size);

    void FreeVirtualMemory(void* pointer, uint64_t size);

    void* Allocate4KPages(uint64_t amount);
    void* Allocate2MPages(uint64_t amount);
    void* Allocate1GPages(uint64_t amount);
    void* KernelAllocate4KPages(uint64_t amount);
    void* KernelAllocate2MPages(uint64_t amount);
    void* KernelAllocate1GPages(uint64_t amount);

    void MapVirtualMemory4K(uint32_t phys, uint32_t virt);
    void MapVirtualMemory2M(uint32_t phys, uint32_t virt);
    void MapVirtualMemory1G(uint32_t phys, uint32_t virt);

    void SwitchPageDirectory(uint64_t phys);

    inline void SetPageFrame(uint64_t* page, uint64_t addr){
        *page = (*page & ~PAGE_FRAME) | addr;
    }

    inline void SetPageFlags(uint64_t* page, uint64_t flags){
        *page |= flags;
    }
}