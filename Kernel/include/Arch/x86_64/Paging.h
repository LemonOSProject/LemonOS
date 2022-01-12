#pragma once

#include <CPU.h>
#include <stdint.h>

#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ULL
#define IO_VIRTUAL_BASE (KERNEL_VIRTUAL_BASE - 0x100000000ULL) // KERNEL_VIRTUAL_BASE - 4GB

#define PML4_GET_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_GET_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PAGE_DIR_GET_INDEX(addr) (((addr) >> 21) & 0x1FF)
#define PAGE_TABLE_GET_INDEX(addr) (((addr) >> 12) & 0x1FF)

#define PML4_PRESENT 1
#define PML4_WRITABLE (1 << 1)
#define PML4_FRAME 0xFFFFFFFFFF000

#define PDPT_PRESENT 1
#define PDPT_WRITABLE (1 << 1)
#define PDPT_1G (1 << 7)
#define PDPT_USER (1 << 2)
#define PDPT_FRAME 0xFFFFFFFFFF000

#define PDE_PRESENT 1
#define PDE_WRITABLE (1 << 1)
#define PDE_USER (1 << 2)
#define PDE_CACHE_DISABLED (1 << 4)
#define PDE_2M (1 << 7)
#define PDE_FRAME 0xFFFFFFFFFF000
#define PDE_PAT (1 << 12)

#define PAGE_PRESENT 1
#define PAGE_WRITABLE (1 << 1)
#define PAGE_USER (1 << 2)
#define PAGE_WRITETHROUGH (1 << 3)
#define PAGE_CACHE_DISABLED (1 << 4)
#define PAGE_FRAME 0xFFFFFFFFFF000ULL
#define PAGE_PAT (1 << 7)
#define PAGE_PAT_WRITE_COMBINING                                                                                       \
    (PAGE_PAT | PAGE_CACHE_DISABLED |                                                                                  \
     PAGE_WRITETHROUGH) // We set PA7 to write combining, PAGE_PAT is the high bit of the PAT index

#define PAGE_SIZE_4K 4096U
#define PAGE_SIZE_2M 0x200000U
#define PAGE_SIZE_1G 0x40000000ULL
#define PDPT_SIZE 0x8000000000U

#define PAGES_PER_TABLE 512
#define TABLES_PER_DIR 512
#define DIRS_PER_PDPT 512
#define PDPTS_PER_PML4 512

#define MAX_PDPT_INDEX 511

#define PAGE_SHIFT_4K 12
#define PAGE_COUNT_4K(size) (((size) + (PAGE_SIZE_4K - 1)) >> 12)

typedef uint64_t page_t;
typedef uint64_t pd_entry_t;
typedef uint64_t pdpt_entry_t;
typedef uint64_t pml4_entry_t;

typedef struct {
    uint64_t phys;
    page_t* virt;
} __attribute__((packed)) page_table_t;

using page_dir_t = pd_entry_t[TABLES_PER_DIR];
using pdpt_t = pdpt_entry_t[DIRS_PER_PDPT];
using pml4_t = pml4_entry_t[PDPTS_PER_PML4];

typedef struct PageMap { // Each process will have a maximum of 96GB of virtual memory.
    pdpt_entry_t* pdpt;  // 512GB is more than ample
    pd_entry_t** pageDirs;
    uint64_t* pageDirsPhys;
    page_t*** pageTables;
    pml4_entry_t* pml4;
    uint64_t pdptPhys;
    uint64_t pml4Phys;
} __attribute__((packed)) page_map_t;

// Allows handling of page faults without kernel panic
struct PageFaultTrap {
    uintptr_t instructionPointer;
    void (*handler)();
};

class AddressSpace;
namespace Memory {
extern pml4_t kernelPML4;

// Creates a new pagemap object
PageMap* CreatePageMap();
// Clones an existing pagemap object
PageMap* ClonePageMap(PageMap* pageMap);
// Destroy pagemap
void DestroyPageMap(PageMap* pageMap);

void InitializeVirtualMemory();
void LateInitializeVirtualMemory();

void* KernelAllocate4KPages(uint64_t amount);

void Free4KPages(void* addr, uint64_t amount, page_map_t* addressSpace);
void KernelFree4KPages(void* addr, uint64_t amount);
void FreeVirtualMemory(void* pointer, uint64_t size);

/////////////////////////////
/// \brief Map 4KB Pages
///
/// \param phys Physical address to map to
/// \param virt Virtual address of the mapping
/// \param amount Amount of pages to map
/////////////////////////////
void KernelMapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount);

/////////////////////////////
/// \brief Map 4KB Pages
///
/// \param phys Physical address to map to
/// \param virt Virtual address of the mapping
/// \param amount Amount of pages to map
/// \param flags Page Flags
/////////////////////////////
void KernelMapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount, uint64_t flags);

/////////////////////////////
/// \brief Map 4KB Pages
///
/// \param phys Physical address to map to
/// \param virt Virtual address of the mapping
/// \param amount Amount of pages to map
/// \param pageMap PageMap to map pages
/////////////////////////////
void MapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount, PageMap* pageMap);

/////////////////////////////
/// \brief Map 4KB Pages
///
/// \param phys Physical address to map to
/// \param virt Virtual address of the mapping
/// \param amount Amount of pages to map
/// \param flags Page Flags
/// \param pageMap PageMap to map pages
/////////////////////////////
void MapVirtualMemory4K(uint64_t phys, uint64_t virt, uint64_t amount, uint64_t flags, PageMap* pageMap);

uintptr_t GetIOMapping(uintptr_t addr);

bool CheckKernelPointer(uintptr_t addr, uint64_t len);
bool CheckUsermodePointer(uintptr_t addr, uint64_t len, AddressSpace* addressSpace);
uint64_t VirtualToPhysicalAddress(uint64_t addr);
uint64_t VirtualToPhysicalAddress(uint64_t addr, page_map_t* addressSpace);

void SwitchPageDirectory(uint64_t phys);

void RegisterPageFaultTrap(PageFaultTrap trap);
void PageFaultHandler(void*, RegisterContext* regs);

inline void SetPageFrame(uint64_t* page, uint64_t addr) { *page = (*page & ~PAGE_FRAME) | (addr & PAGE_FRAME); }

inline void SetPageFlags(uint64_t* page, uint64_t flags) { *page |= flags; }

inline uint32_t GetPageFrame(uint64_t p) { return (p & PAGE_FRAME) >> 12; }

inline void invlpg(uintptr_t addr) { asm volatile("invlpg (%0)" ::"r"(addr)); }
} // namespace Memory