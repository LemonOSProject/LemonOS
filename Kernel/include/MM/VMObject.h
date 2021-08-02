#pragma once

#include <stdint.h>
#include <stddef.h>

#include <Compiler.h>
#include <Lock.h>
#include <Paging.h>
#include <RefPtr.h>

#define PHYS_BLOCK_MAX (0xffffffff << PAGE_SHIFT_4K)

class VMObject {
    friend class AddressSpace;
    friend void ::Memory::PageFaultHandler(void*, struct RegisterContext*);
public:
    VMObject(size_t size, bool anonymous, bool shared);
    virtual ~VMObject() = default;

    virtual int Hit(uintptr_t base, uintptr_t offset, PageMap* pMap);
    virtual void MapAllocatedBlocks(uintptr_t base, PageMap* pMap) = 0;

    virtual VMObject* Clone() = 0;
    virtual VMObject* Split(uintptr_t offset);

    ALWAYS_INLINE size_t Size() const { return size; }
    virtual size_t UsedPhysicalMemory() const { return 0; }

    ALWAYS_INLINE bool IsAnonymous() const { return anonymous; }
    ALWAYS_INLINE bool IsShared() const { return shared; }
    ALWAYS_INLINE bool IsCopyOnWrite() const { return copyOnWrite; }
    ALWAYS_INLINE bool IsReclaimable() const { return reclaimable; }

    ALWAYS_INLINE virtual bool CanMunmap() const { return false; }
    ALWAYS_INLINE size_t ReferenceCount() const { return refCount; }
protected:
    size_t size;

    size_t refCount = 0; // References to the VM object

    bool anonymous : 1 = true;
    bool shared : 1 = false;
    bool copyOnWrite : 1 = false;
    bool reclaimable : 1 = false;
};

// VMObject that maps to allocated physical pages (as opposed to MMIO, etc.)
class PhysicalVMObject : public VMObject {
public:
    PhysicalVMObject(size_t size, bool anonymous, bool shared);
    virtual ~PhysicalVMObject();

    int Hit(uintptr_t base, uintptr_t offset, PageMap* pMap) final;
    void ForceAllocate(); // Force allocate all blocks
    virtual void MapAllocatedBlocks(uintptr_t base, PageMap* pMap);

    virtual VMObject* Clone();

    virtual size_t UsedPhysicalMemory() const;

protected:
    uint32_t* physicalBlocks = nullptr; // A bit of an optimization, since one physical block is 4KB, we can shift by 12
};

class ProcessImageVMObject final : public PhysicalVMObject {
public:
    ProcessImageVMObject(uintptr_t base, size_t size, bool write);

    void MapAllocatedBlocks(uintptr_t base, PageMap* pMap);
protected:
    bool write : 1 = true;

    uintptr_t base;
};

class AnonymousVMObject : public PhysicalVMObject{
    AnonymousVMObject(size_t size);

    VMObject* Split(uintptr_t offset) override;

    ALWAYS_INLINE bool CanMunmap() const override { return true; }
};

struct MappedRegion {
    uintptr_t base;
    size_t size;
    FancyRefPtr<VMObject> vmObject;
    ReadWriteLock lock; // Prevents the region being deallocated whilst in use

    ALWAYS_INLINE uintptr_t Base() const { return base; }
    ALWAYS_INLINE uintptr_t Size() const { return size; }
    ALWAYS_INLINE uintptr_t End() const { return base + size; }

    MappedRegion() = delete;
    ALWAYS_INLINE MappedRegion(uintptr_t base, size_t size)
        : base(base), size(size), vmObject(nullptr) {

    }

    ALWAYS_INLINE MappedRegion(uintptr_t base, size_t size, FancyRefPtr<VMObject> vmObject)
        : base(base), size(size), vmObject(vmObject) {

    }

    ALWAYS_INLINE MappedRegion(const MappedRegion& region){
        base = region.base;
        size = region.size;
        vmObject = region.vmObject;

        lock = ReadWriteLock();
    }

    ALWAYS_INLINE MappedRegion& operator=(const MappedRegion& region){
        base = region.base;
        size = region.size;
        vmObject = region.vmObject;

        lock = ReadWriteLock();
        return *this;
    }

    ALWAYS_INLINE MappedRegion(MappedRegion&& region){
        base = region.base;
        size = region.size;
        vmObject = std::move(region.vmObject);

        region.base = region.size = 0;
    }

    ALWAYS_INLINE MappedRegion& operator=(MappedRegion&& region){
        base = region.base;
        size = region.size;
        vmObject = std::move(region.vmObject);

        region.base = region.size = 0;

        return *this;
    }
};