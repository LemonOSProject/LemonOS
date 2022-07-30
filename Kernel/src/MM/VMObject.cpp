#include <MM/VMObject.h>

#include <Paging.h>
#include <PhysicalAllocator.h>
#include <Scheduler.h>
#include <CPU.h>

#include <Assert.h>

VMObject::VMObject(size_t size, bool anonymous, bool shared) : size(size), anonymous(anonymous), shared(shared) {
    assert(!(size & (PAGE_SIZE_4K - 1)));
}

int VMObject::Hit(uintptr_t, uintptr_t, PageMap*){
    return 1; // Fatal page fault, kill process
}

VMObject* VMObject::Split(uintptr_t offset){
    assert(!"Cannot split VMObject!");

    return nullptr;
}

PhysicalVMObject::PhysicalVMObject(uintptr_t size, bool anonymous, bool shared) : VMObject(size, anonymous, shared) {
    assert(!(size & (PAGE_SIZE_4K - 1)));

    size_t blockCount = PAGE_COUNT_4K(size);

    physicalBlocks = new uint32_t[blockCount];

    if(anonymous){
        memset(physicalBlocks, 0, sizeof(uint32_t) * blockCount);
    } else {
        void* mapping = Memory::KernelAllocate4KPages(1);
        for(unsigned i = 0; i < blockCount; i++){
            uintptr_t phys = Memory::AllocatePhysicalMemoryBlock();
            physicalBlocks[i] = phys >> PAGE_SHIFT_4K; // Allocate all of our blocks

            Memory::KernelMapVirtualMemory4K(phys, (uintptr_t)mapping, 1);
            memset(mapping, 0, PAGE_SIZE_4K);
        }
        Memory::KernelFree4KPages(mapping, 1);
    }
}

int PhysicalVMObject::Hit(uintptr_t base, uintptr_t offset, PageMap* pMap){
    unsigned blockIndex = offset >> PAGE_SHIFT_4K;
    assert(blockIndex < (size >> PAGE_SHIFT_4K));

    uint32_t& block = physicalBlocks[blockIndex];
    if(block){ // Another reference to the VMObject probably mapped this block
        Memory::MapVirtualMemory4K(static_cast<uintptr_t>(block) << PAGE_SHIFT_4K, base + offset, 1, pMap);
    } else { // We need to allocate block
        assert(anonymous);

        uintptr_t phys = Memory::AllocatePhysicalMemoryBlock();
        assert(phys < PHYS_BLOCK_MAX);
        if(!phys){
            return 1; // Failed to allocate
        }

        block = phys >> PAGE_SHIFT_4K;

        Memory::MapVirtualMemory4K(phys, base + offset, 1, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT, pMap);
        
        if(GetCR3() == pMap->pml4Phys){
            memset(reinterpret_cast<void*>((base + offset) & ~static_cast<uintptr_t>(PAGE_SIZE_4K - 1)), 0, PAGE_SIZE_4K); // Zero the block
        } else {
            void* mapping = Memory::KernelAllocate4KPages(1);
            Memory::KernelMapVirtualMemory4K(phys, (uintptr_t)mapping, 1);

            memset(mapping, 0, PAGE_SIZE_4K);

            Memory::KernelFree4KPages(mapping, 1);
        }
    }

    return 0; // Success
}

void PhysicalVMObject::ForceAllocate(){
    void* mapping = Memory::KernelAllocate4KPages(1);
    for(unsigned i = 0; i < (size >> PAGE_SHIFT_4K); i++){
        if(physicalBlocks[i]){
            continue; // Already allocated
        }
        assert(anonymous);

        uintptr_t phys = Memory::AllocatePhysicalMemoryBlock();
        assert(phys < PHYS_BLOCK_MAX);
        
        physicalBlocks[i] = phys >> PAGE_SHIFT_4K;

        Memory::KernelMapVirtualMemory4K(phys, (uintptr_t)mapping, 1);
        memset(mapping, 0, PAGE_SIZE_4K);
    }
    Memory::KernelFree4KPages(mapping, 1);
}

void PhysicalVMObject::MapAllocatedBlocks(uintptr_t base, PageMap* pMap){
    uintptr_t virt = base;

    long pgFlags = PAGE_USER | (PAGE_WRITABLE * (!copyOnWrite)) | PAGE_PRESENT;
    for(unsigned i = 0; i < (size >> PAGE_SHIFT_4K); i++){
        uint64_t block = physicalBlocks[i];
        if(block){ // Is it allocated?
            // Only set write flag if copyOnWrite is false
            Memory::MapVirtualMemory4K(block << PAGE_SHIFT_4K, virt, 1, pgFlags, pMap);
        } else {
            Memory::MapVirtualMemory4K(0, virt, 1, PAGE_USER, pMap); // Mark as user, not present, not writable
        }

        virt += PAGE_SIZE_4K;
    }
}

VMObject* PhysicalVMObject::Clone(){
    assert(!shared);
    PhysicalVMObject* newVMO = new PhysicalVMObject(size, anonymous, shared);

    // Temporary mappings so we can copy the data over
    uint8_t* virtBuffer = reinterpret_cast<uint8_t*>(Memory::KernelAllocate4KPages(2));
    uint8_t* virtDestBuffer = virtBuffer + PAGE_SIZE_4K;

    for(unsigned i = 0; i < (size >> PAGE_SHIFT_4K); i++){
        uintptr_t block = physicalBlocks[i];
        if(block){
            uintptr_t newBlock = Memory::AllocatePhysicalMemoryBlock();
            newVMO->physicalBlocks[i] = newBlock >> PAGE_SHIFT_4K;

            Memory::KernelMapVirtualMemory4K(block << PAGE_SHIFT_4K, (uintptr_t)virtBuffer, 1); // Map temporary mappings to our blocks
            Memory::KernelMapVirtualMemory4K(newBlock, (uintptr_t)virtDestBuffer, 1);

            memcpy(virtDestBuffer, virtBuffer, PAGE_SIZE_4K); // Copy each block
        }
    }

    Memory::KernelFree4KPages(virtBuffer, 2);

    newVMO->copyOnWrite = false;
    newVMO->refCount = 1;

    return newVMO;
}

size_t PhysicalVMObject::UsedPhysicalMemory() const {
    if(!anonymous){
        return size;
    }

    unsigned blockCount = 0;
    for(unsigned i = 0; i < (size >> PAGE_SHIFT_4K); i++){
        if(physicalBlocks[i]){
            blockCount++;
        }
    }

    return blockCount << PAGE_SHIFT_4K;
}

PhysicalVMObject::~PhysicalVMObject(){
    assert(refCount <= 1); // Make sure someone isn't trying to free the VMO with more than 1 reference left

    if(physicalBlocks){
        for(unsigned i = 0; i < size >> PAGE_SHIFT_4K; i++){ // Free our allocated physical blocks
            if(physicalBlocks[i]){
                Memory::FreePhysicalMemoryBlock(static_cast<uintptr_t>(physicalBlocks[i]) << PAGE_SHIFT_4K);
            }
            
        }

        delete[] physicalBlocks;
    }
}

ProcessImageVMObject::ProcessImageVMObject(uintptr_t base, size_t size, bool write) :
    PhysicalVMObject(size, false, false), write(write), base(base) {

}

void ProcessImageVMObject::MapAllocatedBlocks(uintptr_t requestedBase, PageMap* pMap){
    assert(requestedBase == base);

    uintptr_t virt = base;
    for(unsigned i = 0; i < (size >> PAGE_SHIFT_4K); i++){
        uint64_t block = physicalBlocks[i];
        assert(block);

        Memory::MapVirtualMemory4K(block << PAGE_SHIFT_4K, virt, 1, PAGE_USER | (PAGE_WRITABLE * (write && !copyOnWrite)) | PAGE_PRESENT, pMap);
        virt += PAGE_SIZE_4K;
    }
}

AnonymousVMObject::AnonymousVMObject(size_t size)
    : PhysicalVMObject(size, true, false) {

}

VMObject* AnonymousVMObject::Split(uintptr_t offset){
    assert(!(offset & (PAGE_SIZE_4K - 1)));
    assert(!copyOnWrite || refCount <= 1);

    uintptr_t offsetBlocks = offset >> PAGE_SHIFT_4K;

    AnonymousVMObject* newObject = new AnonymousVMObject(size - offset);
    memcpy(newObject->physicalBlocks, &physicalBlocks[offsetBlocks], ((size - offset) >> PAGE_SHIFT_4K) * sizeof(uint32_t));

    uint32_t* newPhysicalBlocks = new uint32_t[offsetBlocks];
    memcpy(newPhysicalBlocks, physicalBlocks, offsetBlocks * sizeof(uint32_t));

    delete physicalBlocks;
    physicalBlocks = newPhysicalBlocks;
    size = offset;

    return newObject;    
}