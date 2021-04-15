#include <MM/AddressSpace.h>

#include <StackTrace.h>
#include <CPU.h>

AddressSpace::AddressSpace(PageMap* pm) : pageMap(pm) {
    
}

AddressSpace::~AddressSpace(){
    IF_DEBUG((debugLevelUsermodeMM >= DebugLevelNormal), {
        Log::Info("Destroying address space with %u regions.", regions.get_length());
    });
    regions.clear(); // Let FancyRefPTr handle cleanup for us

    Memory::DestroyPageMap(pageMap);
}

MappedRegion* AddressSpace::AddressToRegion(uintptr_t address){
    ScopedSpinLock acquired(lock);

    for(MappedRegion& region : regions){
        if(!region.vmObject.get()){
            continue;
        }

        if(address >= region.Base() && address < region.End()){
            region.lock.AcquireRead();
            return &region;
        }
    }

    return nullptr;
}

bool AddressSpace::RangeInRegion(uintptr_t base, size_t size){
    uintptr_t end = base + size;

    auto it = regions.begin();
    for(; it != regions.end(); it++){
        if(base < it->Base()){
            IF_DEBUG((debugLevelUsermodeMM >= DebugLevelNormal), {
                Log::Warning("range (%x-%x) not in region (%x-%x)!", base, base + size, it->Base(), it->End());
                PrintStackTrace(GetRBP());
            });
            return false; // Outside of the region, regions are ordered
        } else if(base >= it->Base() && base < it->End()){ // We intersect with this region
            base = it->End();
        }

        if(base >= end){
            return true; // Range lies completely within the region
        }
    }

    if(base >= end){
        return true; // Range lies completely within the region
    }

    return false;
}

MappedRegion* AddressSpace::MapVMO(FancyRefPtr<VMObject> obj, uintptr_t base, bool fixed){
    assert(!(obj->Size() & (PAGE_SIZE_4K - 1)));
    assert(!(base & (PAGE_SIZE_4K - 1)));

    ScopedSpinLock acquired(lock);

    MappedRegion* region;
    if(base && (region = AllocateRegionAt(base, obj->Size()))){
        region->vmObject = nullptr;
    } else if(fixed){ // Could not create region at base
        IF_DEBUG((debugLevelUsermodeMM >= DebugLevelNormal), {
            Log::Warning("Fixed region (%x - %x) was in use, we do not yet overwrite existing regions for fixed mappings.", base, obj->Size());
            for(MappedRegion& region : regions){
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

    obj->MapAllocatedBlocks(region->Base(), pageMap);

    return region;
}

MappedRegion* AddressSpace::AllocateAnonymousVMObject(size_t size, uintptr_t base, bool fixed){
    assert(!(size & (PAGE_SIZE_4K - 1)));
    assert(!(base & (PAGE_SIZE_4K - 1)));

    ScopedSpinLock acquired(lock);

    MappedRegion* region;
    if(base && (region = AllocateRegionAt(base, size))){
        region->vmObject = nullptr;
    } else if(fixed){ // Could not create region at base
        Log::Warning("Fixed region (%x - %x) was in use, we do not yet overwrite existing regions for fixed mappings.", base, size);
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

    vmo->MapAllocatedBlocks(region->Base(), pageMap);

    return region;
}

AddressSpace* AddressSpace::Fork(){
    ScopedSpinLock acquired(lock);

    AddressSpace* fork = new AddressSpace(Memory::ClonePageMap(pageMap));
    for(auto it = regions.begin(); it != regions.end(); it++){
        MappedRegion& r = *it;

        r.vmObject->refCount++;
        if(!r.vmObject->IsShared()){ // Shared VM Objects are shared, we do not want COW
            r.vmObject->copyOnWrite = true;

            // Remap for both us and the fork as we are no longer setting the write flag because of COW
            r.vmObject->MapAllocatedBlocks(r.Base(), pageMap);
        }

        fork->regions.add_back(const_cast<const MappedRegion&>(r));
        
        r.vmObject->MapAllocatedBlocks(r.Base(), fork->pageMap);
    }

    fork->parent = this;

    return fork;
}

long AddressSpace::UnmapMemory(uintptr_t base, size_t size){
    uintptr_t end = base + size;
    ScopedSpinLock acquired(lock);

    retry:
    for(auto it = regions.begin(); it != regions.end(); it++){
        MappedRegion& region = *it;
        if(!region.vmObject){
            // Assume vmobject has been removed
            regions.remove(it);
            goto retry;
        }

        if(!region.vmObject->CanMunmap()){
            continue;
        }

        if(region.Base() >= base && region.Base() <= end){
            region.lock.AcquireWrite();

            if(region.End() <= end){ // Whole region within our range
                regions.remove(it);
                goto retry;
            }
        }
    }
    Memory::MapVirtualMemory4K(0, base, PAGE_COUNT_4K(size), 0, pageMap);

    return 0;
}

void AddressSpace::UnmapAll() {
    ScopedSpinLock acq(lock);

    for(MappedRegion& r : regions){
        Memory::MapVirtualMemory4K(0, r.Base(), PAGE_COUNT_4K(r.Size()), 0, pageMap);
    }

    regions.clear();
}

size_t AddressSpace::UsedPhysicalMemory() const {
    size_t mem = 0;

    for(MappedRegion& region : regions){
        if(region.vmObject.get()){
            mem += region.vmObject->UsedPhysicalMemory();
        }
    }

    return mem;
}

void AddressSpace::DumpRegions(){
    for(MappedRegion& region : regions){
        if(!region.vmObject.get()) continue;
        Log::Info("region (%x - %x) anon? %Y, shared? %Y, COW? %Y", region.Base(), region.End(), region.vmObject->IsAnonymous(), region.vmObject->IsShared(), region.vmObject->IsCopyOnWrite());
    }
}

MappedRegion* AddressSpace::FindAvailableRegion(size_t size){
    uintptr_t base = PAGE_SIZE_4K; // We do not want zero addresses
    uintptr_t end = base + size;

    auto it = regions.begin();
    for(; it != regions.end(); it++){
        if(it->Base() >= end){
            return &regions.insert(MappedRegion(base, size), it);
        }

        if(base >= it->Base() && base < it->End()){ // We intersect with this region
            base = it->End();
            end = base + size;
        }
        
        if(end > it->Base() && end <= it->End()){ // We intersect with this region
            base = it->End();
            end = base + size;
        }

        if(base < it->Base() && end > it->End()){ // We encapsulate this region
            base = it->End();
            end = base + size;
        }
    }

    if(end < KERNEL_VIRTUAL_BASE){
        return &regions.add_back(MappedRegion(base, size));
    }

    return nullptr; // Failed to allocate
}

MappedRegion* AddressSpace::AllocateRegionAt(uintptr_t base, size_t size){
    uintptr_t end = base + size;

    auto it = regions.begin();
    auto lastIt = regions.end();
    for(; it != regions.end(); it++){
        if(it->End() <= base){
            lastIt = it;
            continue;
        }

        if(it->Base() >= end){
            return &regions.insert(MappedRegion(base, size), it);
        }

        IF_DEBUG((debugLevelUsermodeMM >= DebugLevelNormal), {
            Log::Error("AllocateRegionAt: Failed at %x - %x", it->Base(), it->End());
        });
        return nullptr;
    }

    if(!regions.get_length() || regions.get_back().End() <= base){
        return &regions.add_back(MappedRegion(base, size)); // Region is at end
    }

    return nullptr;
}