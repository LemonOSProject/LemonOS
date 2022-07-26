#include <MM/AddressSpace.h>

#include <CPU.h>
#include <StackTrace.h>

AddressSpace::AddressSpace(PageMap* pm) : m_pageMap(pm) {}

AddressSpace::~AddressSpace() {
    IF_DEBUG((debugLevelUsermodeMM >= DebugLevelNormal),
             { Log::Info("Destroying address space %x with %u regions.", this, m_regions.get_length()); });

    for (auto& region : m_regions) {
        if (region.vmObject) {
            region.vmObject->refCount--;
        }
    }
    m_regions.clear(); // Let FancyRefPtr handle cleanup for us

    Memory::DestroyPageMap(m_pageMap);
}

MappedRegion* AddressSpace::AddressToRegionReadLock(uintptr_t address) {
    ScopedSpinLock acquired(m_lock);

    for (MappedRegion& region : m_regions) {
        if (!region.vmObject.get()) {
            continue;
        }

        if (address >= region.Base() && address < region.End()) {
            region.lock.AcquireRead();
            return &region;
        }
    }

    return nullptr;
}

MappedRegion* AddressSpace::AddressToRegionWriteLock(uintptr_t address) {
    ScopedSpinLock acquired(m_lock);

    for (MappedRegion& region : m_regions) {
        if (!region.vmObject.get()) {
            continue;
        }

        if (address >= region.Base() && address < region.End()) {
            region.lock.AcquireWrite();
            return &region;
        }
    }

    return nullptr;
}

bool AddressSpace::RangeInRegion(uintptr_t base, size_t size) {
    uintptr_t end = base + size;

    auto it = m_regions.begin();
    for (; it != m_regions.end(); it++) {
        if (base < it->Base()) {
            IF_DEBUG((debugLevelUsermodeMM >= DebugLevelNormal), {
                Log::Warning("range (%x-%x) not in region (%x-%x)!", base, base + size, it->Base(), it->End());
                PrintStackTrace(GetRBP());
            });
            return false;                                    // Outside of the region, regions are ordered
        } else if (base >= it->Base() && base < it->End()) { // We intersect with this region
            base = it->End();
        }

        if (base >= end) {
            return true; // Range lies completely within the region
        }
    }

    if (base >= end) {
        return true; // Range lies completely within the region
    }

    return false;
}

long AddressSpace::UnmapRegion(MappedRegion* region) {
    ScopedSpinLock acquired(m_lock);
    InterruptDisabler disableInterrupts;

    assert(region->lock.IsWriteLocked());

    for (auto it = m_regions.begin(); it != m_regions.end(); it++) {
        if (it->Base() == region->Base()) {
            if(IsKernel()){
                assert(it->Base() >= KERNEL_VIRTUAL_BASE);
                Memory::KernelMapVirtualMemory4K(0, region->Base(), PAGE_COUNT_4K(region->Size()), 0);
            } else {
                Memory::MapVirtualMemory4K(0, region->Base(), PAGE_COUNT_4K(region->Size()), 0, m_pageMap);
            }

            if (it->vmObject) {
                it->vmObject->refCount--;
            }

            m_regions.remove(it);
            return 0;
        }
    }

    Log::Warning("Failed to unmap region object!");
    return 1;
}

MappedRegion* AddressSpace::MapVMO(FancyRefPtr<VMObject> obj, uintptr_t base, bool fixed) {
    assert(!(obj->Size() & (PAGE_SIZE_4K - 1)));
    assert(!(base & (PAGE_SIZE_4K - 1)));

    MappedRegion* region;
    
    ScopedSpinLock acquired(m_lock);
    if (base && (region = AllocateRegionAt(base, obj->Size()))) {
        region->vmObject = nullptr;
    } else if (fixed) { // Could not create region at base
        IF_DEBUG((debugLevelUsermodeMM >= DebugLevelNormal), {
            Log::Warning(
                "Fixed region (%x - %x) was in use, we do not yet overwrite existing regions for fixed mappings.", base,
                base + obj->Size());
            for (MappedRegion& region : m_regions) {
                Log::Info("region (%x - %x)", region.Base(), region.End());
            }
        });
        return nullptr;
    } else {
        region = FindAvailableRegion(obj->Size());
        assert(region);
    }

    assert(region && region->Base());

    obj->refCount++;
    region->vmObject = obj;

    obj->MapAllocatedBlocks(region->Base(), m_pageMap);

    return region;
}

MappedRegion* AddressSpace::AllocateAnonymousVMObject(size_t size, uintptr_t base, bool fixed) {
    assert(!(size & (PAGE_SIZE_4K - 1)));
    assert(!(base & (PAGE_SIZE_4K - 1)));

    MappedRegion* region;
    ScopedSpinLock acquired(m_lock);

    if (base && (region = AllocateRegionAt(base, size))) {
        region->vmObject = nullptr;
    } else if (fixed) { // Could not create region at base
        Log::Warning("Fixed region (%x - %x) was in use, we do not yet overwrite existing regions for fixed mappings.",
                    base, size);
        return nullptr;
    } else {
        region = FindAvailableRegion(size);
    }

    assert(region && region->Base());

    PhysicalVMObject* vmo = new PhysicalVMObject(size, true, false);
    vmo->anonymous = true;
    vmo->copyOnWrite = false;
    vmo->refCount = 1;

    region->vmObject = vmo;

    vmo->MapAllocatedBlocks(region->Base(), m_pageMap);

    return region;
}

AddressSpace* AddressSpace::Fork() {
    ScopedSpinLock acquired(m_lock);

    AddressSpace* fork = new AddressSpace(Memory::ClonePageMap(m_pageMap));
    for (auto it = m_regions.begin(); it != m_regions.end(); it++) {
        MappedRegion& r = *it;

        r.vmObject->refCount++;
        if (!r.vmObject->IsShared()) { // Shared VM Objects are shared, we do not want COW
            r.vmObject->copyOnWrite = true;

            // Remap for both us and the fork as we are no longer setting the write flag because of COW
            r.vmObject->MapAllocatedBlocks(r.Base(), m_pageMap);
        }

        fork->m_regions.add_back(const_cast<const MappedRegion&>(r));

        r.vmObject->MapAllocatedBlocks(r.Base(), fork->m_pageMap);
    }

    fork->m_parent = this;

    return fork;
}

long AddressSpace::UnmapMemory(uintptr_t base, size_t size) {
    uintptr_t end = base + size;
    ScopedSpinLock acquired(m_lock);

retry:
    for (auto it = m_regions.begin(); it != m_regions.end(); it++) {
        MappedRegion& region = *it;
        region.lock.AcquireWrite();

        if (!region.vmObject) {
            Memory::MapVirtualMemory4K(0, base, PAGE_COUNT_4K(size), 0, m_pageMap);

            // Assume vmobject has been removed
            m_regions.remove(it);
            goto retry;
        }

        if (region.Base() >= base && region.Base() <= end) {
            if (region.End() <= end) { // Whole region within our range
                if (region.vmObject) {
                    it->vmObject->refCount--;
                }

                Memory::MapVirtualMemory4K(0, base, PAGE_COUNT_4K(size), 0, m_pageMap);

                m_regions.remove(it);
                goto retry;
            }
        }

        region.lock.ReleaseWrite();
    }

    return 0;
}

size_t AddressSpace::UsedPhysicalMemory() const {
    size_t mem = 0;

    for (MappedRegion& region : m_regions) {
        if (region.vmObject.get()) {
            mem += region.vmObject->UsedPhysicalMemory();
        }
    }

    return mem;
}

void AddressSpace::DumpRegions() {
    for (MappedRegion& region : m_regions) {
        if (!region.vmObject.get())
            continue;
        Log::Info("region (%x - %x) anon? %Y, shared? %Y, COW? %Y", region.Base(), region.End(),
                  region.vmObject->IsAnonymous(), region.vmObject->IsShared(), region.vmObject->IsCopyOnWrite());
    }
}

MappedRegion* AddressSpace::FindAvailableRegion(size_t size) {
    uintptr_t base = PAGE_SIZE_4K; // We do not want zero addresses
    uintptr_t end = base + size;

    auto it = m_regions.begin();
    for (; it != m_regions.end(); it++) {
        if (it->Base() >= end) {
            return &m_regions.insert(MappedRegion(base, size), it);
        }

        if (base >= it->Base() && base < it->End()) { // We intersect with this region
            base = it->End();
            end = base + size;
        }

        if (end > it->Base() && end <= it->End()) { // We intersect with this region
            base = it->End();
            end = base + size;
        }

        if (base < it->Base() && end > it->End()) { // We encapsulate this region
            base = it->End();
            end = base + size;
        }
    }

    if (end < m_endRegion) {
        return &m_regions.add_back(MappedRegion(base, size));
    }

    return nullptr; // Failed to allocate
}

MappedRegion* AddressSpace::AllocateRegionAt(uintptr_t base, size_t size) {
    uintptr_t end = base + size;

    auto it = m_regions.begin();
    auto lastIt = m_regions.end();
    for (; it != m_regions.end(); it++) {
        if (it->End() <= base) {
            lastIt = it;
            continue;
        }

        if (it->Base() >= end) {
            return &m_regions.insert(MappedRegion(base, size), it);
        }

        IF_DEBUG((debugLevelUsermodeMM >= DebugLevelNormal),
                 { Log::Error("AllocateRegionAt: Failed at %x - %x", it->Base(), it->End()); });
        return nullptr;
    }

    if (!m_regions.get_length() || m_regions.get_back().End() <= base) {
        return &m_regions.add_back(MappedRegion(base, size)); // Region is at end
    }

    return nullptr;
}