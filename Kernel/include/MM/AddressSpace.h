#pragma once

#include <Paging.h>
#include <Vector.h>
#include <Lock.h>
#include <Pair.h>
#include <RefPtr.h>

#include <MM/VMObject.h>

class AddressSpace final {
public:
    AddressSpace(PageMap* pm);
    ~AddressSpace();

    /////////////////////////////
    /// \brief Find and read lock a region from an address
    ///
    /// Due to the lock, the region cannot be deallocated until this thread and all others release their read locks.
    ///
    /// \param address Address of region
    ///
    /// \return Locked region on success, nullptr on failure
    /////////////////////////////
    MappedRegion* AddressToRegion(uintptr_t address);

    /////////////////////////////
    /// \brief Check if the range is in a valid region
    ///
    /// \param base Address of region
    /// \param size Size of region
    ///
    /// \return True if range is valid, false if it lies outside of memory regions
    /////////////////////////////
    bool RangeInRegion(uintptr_t base, size_t size);

    MappedRegion* MapVMO(FancyRefPtr<VMObject> obj, uintptr_t base, bool fixed);
    MappedRegion* AllocateAnonymousVMObject(size_t size, uintptr_t base, bool fixed);
    AddressSpace* Fork();

    long UnmapMemory(uintptr_t base, size_t size);
    void UnmapAll();

    size_t UsedPhysicalMemory() const;
    void DumpRegions();

    __attribute__(( always_inline )) inline PageMap* GetPageMap() { return pageMap; }
protected:
    MappedRegion* FindAvailableRegion(size_t size);
    MappedRegion* AllocateRegionAt(uintptr_t base, size_t size);

    lock_t lock = 0;

    PageMap* pageMap = nullptr;
    List<MappedRegion> regions;

    AddressSpace* parent = nullptr;
};
