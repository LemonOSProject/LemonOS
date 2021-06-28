#include <Memory.h>

#include <Lock.h>
#include <SharedMemory.h>
#include <Scheduler.h>
#include <Logging.h>
#include <RefPtr.h>

SharedVMObject::SharedVMObject(size_t size, int64_t key, pid_t owner, pid_t recipient, bool isPrivate)
    : PhysicalVMObject(size, false, true), key(key), owner(owner), recipient(recipient), isPrivate(isPrivate) {
        
}

namespace Memory {
    lock_t sMemLock = 0;

    Vector<FancyRefPtr<SharedVMObject>> table;

    int64_t NextKey(){
        int64_t key = 1;
        while((key - 1) < table.size() && table[key - 1].get()) key++;

        if(key - 1 >= table.size()){
            table.resize(key);
        }

        return key;
    }

    FancyRefPtr<SharedVMObject> GetSharedMemory(int64_t key){
        int64_t index = key - 1;

        if(index >= 0 && index < table.size()){
            return table[key - 1];
        } else return nullptr;
    }

    int CanModifySharedMemory(pid_t pid, int64_t key){
        FancyRefPtr<SharedVMObject> sMem = nullptr;
        if((sMem = GetSharedMemory(key)).get()){
            if((sMem->Owner() == pid)) return 1;
        }

        return 0;
    }

    int64_t CreateSharedMemory(uint64_t size, uint64_t flags, pid_t owner, pid_t recipient){
        ScopedSpinLock acquired(sMemLock);

        int64_t key = NextKey();
        if(key <= 0) return 0;

        uint64_t vmoSize = (size + PAGE_SIZE_4K - 1) & ~static_cast<size_t>(PAGE_SIZE_4K - 1);

        SharedVMObject* sMem = new SharedVMObject(vmoSize, key, owner, recipient, flags & SMEM_FLAGS_PRIVATE);
        table[key - 1] = sMem;

        return key;
    }

    void* MapSharedMemory(int64_t key, process_t* proc, uint64_t hint){
        ScopedSpinLock acquired(sMemLock);

        FancyRefPtr<SharedVMObject> sMem = GetSharedMemory(key);

        if(!sMem.get()) {
            Log::Warning("Invalid shared memory key %d!", key);
            return nullptr; // Check for invalid key
        }

        if(sMem->IsPrivate()){ // Private Mapping
            if(proc->pid != sMem->Owner() && proc->pid != sMem->Recipient()){
                Log::Warning("Cannot access private mapping!");
                return nullptr; // Does not have access rights
            }
        }

        MappedRegion* region = proc->addressSpace->MapVMO(static_pointer_cast<VMObject>(sMem), hint, false);
        assert(region && region->Base());

        return reinterpret_cast<void*>(region->Base());
    }

    void DestroySharedMemory(int64_t key){
        ScopedSpinLock acquired(sMemLock);
        
        FancyRefPtr<SharedVMObject> sMem = GetSharedMemory(key);
        
        if(!sMem.get()) {
            return; // Check for invalid key
        }

        if(sMem->ReferenceCount() > 0){
            //Log::Error("Will not destroy active shared memory (%u references)", sMem->ReferenceCount());
            return;
        }
        
        table[key - 1] = nullptr; // Keys start at 1
    }
}