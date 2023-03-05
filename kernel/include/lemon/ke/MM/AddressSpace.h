#pragma once

#include <Lock.h>
#include <Paging.h>
#include <Pair.h>
#include <RefPtr.h>
#include <Vector.h>

#include <MM/VMObject.h>

class AddressSpace final {
public:
    AddressSpace(PageMap* pm);
    ~AddressSpace();

    /////////////////////////////
    /// \brief Get Kernel Address Space
    /////////////////////////////
    ALWAYS_INLINE AddressSpace* Kernel();

    /////////////////////////////
    /// \brief Find and read lock a region from an address
    ///
    /// Due to the lock, the region cannot be deallocated until this thread and all others release their read locks.
    ///
    /// \param address Address of region
    ///
    /// \return Locked region on success, nullptr on failure
    /////////////////////////////
    MappedRegion* AddressToRegionReadLock(uintptr_t address);

    /////////////////////////////
    /// \brief Find and write lock a region from an address
    ///
    /// Due to the lock, the region cannot be deallocated or modified until this thread releases the write lock.
    ///
    /// \param address Address of region
    ///
    /// \return Locked region on success, nullptr on failure
    /////////////////////////////
    MappedRegion* AddressToRegionWriteLock(uintptr_t address);

    /////////////////////////////
    /// \brief Find and write lock all regions within this range
    ///
    /// Due to the lock, the region cannot be deallocated or modified until this thread releases the write lock.
    /// Checks whether or not the range has any holes
    ///
    /// \param base Base address of range
    /// \param size Size of range
    /// \param regions All regions found before any holes. Write locks of any regions placed here MUST be released regardless of return
    /// \param protection Indicates the access flags requested on the range
    ///
    /// \return 0 if the whole range is mapped AND can be accessed under \b protection
    /// \return 1 if there is a 'hole' in the range
    /// \return 2 if memory is unaccessible under \b protection
    /////////////////////////////
    int GetRegionsForRangeWriteLock(uintptr_t base, size_t size, Vector<MappedRegion*>* regions, MemoryProtection protection);

    /////////////////////////////
    /// \brief Check if the range is in a valid region
    ///
    /// \param base Address of region
    /// \param size Size of region
    ///
    /// \return True if range is valid, false if it lies outside of memory regions
    /////////////////////////////
    bool RangeInRegion(uintptr_t base, size_t size);

    /////////////////////////////
    /// \brief Unmap a region object
    ///
    /// Unmaps a region object. It is expected that the caller has acquired a write lock
    ///
    /// \return 0 if sucessful
    /////////////////////////////
    long UnmapRegion(MappedRegion* region);

    [[nodiscard]] MappedRegion* MapVMO(FancyRefPtr<VMObject> obj, uintptr_t base, bool fixed);
    MappedRegion* AllocateAnonymousVMObject(size_t size, uintptr_t base, bool fixed);
    AddressSpace* Fork();

    long UnmapMemory(uintptr_t base, size_t size);

    size_t UsedPhysicalMemory() const;
    void DumpRegions();

    ALWAYS_INLINE PageMap* GetPageMap() { return m_pageMap; }

    ALWAYS_INLINE lock_t* GetLock() { return &m_lock; }

protected:
    MappedRegion* FindAvailableRegion(size_t size);
    MappedRegion* AllocateRegionAt(uintptr_t base, size_t size);

    ALWAYS_INLINE bool IsKernel() const { return this == m_kernel; }

    AddressSpace* m_kernel; // Kernel Address Space

    uintptr_t m_startRegion = 0; // Start of the address space (0 for usermode, KERNEL_VIRTUAL_BASE for kernel)
    uintptr_t m_endRegion = KERNEL_VIRTUAL_BASE;   // End of the address space (KERNEL_VIRTUAL_BASE for usermode, UINT64_MAX for kernel)

    lock_t m_lock = 0;

    PageMap* m_pageMap = nullptr;
    List<MappedRegion> m_regions;

    AddressSpace* m_parent = nullptr;
};
