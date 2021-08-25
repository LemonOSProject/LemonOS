#include <frg/slab.hpp>

#include <Assert.h>
#include <CString.h>
#include <Lock.h>
#include <Logging.h>
#include <Paging.h>
#include <PhysicalAllocator.h>

class Lock {
public:
    void lock() { acquireLock(&m_lock); }

    void unlock() { releaseLock(&m_lock); }

private:
    lock_t m_lock = 0;
};

struct KernelAllocator {
    uintptr_t map(size_t len) {
        size_t pageCount = (len + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
        void* ptr = Memory::KernelAllocate4KPages(pageCount);
        uintptr_t base = reinterpret_cast<uintptr_t>(ptr);

        while (pageCount--) {
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), base, 1);
            base += PAGE_SIZE_4K;
        }

        memset(ptr, 0, len);

        return reinterpret_cast<uintptr_t>(ptr);
    }

    void unmap(uintptr_t addr, size_t len) {
        assert(!(addr & (PAGE_SIZE_4K - 1)));

        size_t pageCount = (len + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
        uintptr_t base = addr;

        for (unsigned i = 0; i < pageCount; i++) {
            Memory::FreePhysicalMemoryBlock(Memory::VirtualToPhysicalAddress(base));
            base += PAGE_SIZE_4K;
        }

        Memory::KernelFree4KPages(reinterpret_cast<void*>(addr), pageCount);
    }

    frg::slab_pool<KernelAllocator, Lock> slabPool{*this};
    frg::slab_allocator<KernelAllocator, Lock> slabAllocator{&slabPool};
};

KernelAllocator* allocator = nullptr;

frg::slab_allocator<KernelAllocator, Lock>& Allocator() {
    // This is a hack to get around the allocator not being initialized
    if (!allocator) {
        size_t pageCount = (sizeof(KernelAllocator) + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
        allocator = reinterpret_cast<KernelAllocator*>(Memory::KernelAllocate4KPages(pageCount));
        uintptr_t base = reinterpret_cast<uintptr_t>(allocator);

        while (pageCount--) {
            Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), base, 1);
            base += PAGE_SIZE_4K;
        }

        new (allocator) KernelAllocator;
    }

    return allocator->slabAllocator;
}

void* kmalloc(size_t size) { return Allocator().allocate(size); }

void kfree(void* p) { return Allocator().free(p); }

void* krealloc(void* p, size_t sz) { return Allocator().reallocate(p, sz); }
