#include <memory.h>

#include <lock.h>
#include <sharedmem.h>
#include <scheduler.h>
#include <logging.h>

#define DEFAULT_TABLE_SIZE 65535

namespace Memory {
    int* lock = 0;

    shared_mem_t** table = nullptr;
    unsigned tableSize = 0;

    void InitializeSharedMemory(){
        table = (shared_mem_t**)kmalloc(DEFAULT_TABLE_SIZE * sizeof(shared_mem_t*));
        tableSize = DEFAULT_TABLE_SIZE;
    }

    uint64_t NextKey(){
        uint64_t key = 0;
        while(table[++key]);

        if(key >= tableSize){ // Increase table size
            shared_mem_t** newTable = (shared_mem_t**)kmalloc((tableSize + DEFAULT_TABLE_SIZE) * sizeof(shared_mem_t*));
            memcpy(newTable, table, tableSize * sizeof(shared_mem_t*));

            kfree(table);
            table = newTable;
        }

        return key;
    }

    shared_mem_t* GetSharedMemory(uint64_t key){
        if(key < tableSize){
            return table[key];
        } else return nullptr;
    }

    int CanModifySharedMemory(pid_t pid, uint64_t key){
        shared_mem_t* sMem = nullptr;
        if((sMem = GetSharedMemory(key))){
            if((sMem->owner = pid)) return 1;
        }

        return 0;
    }

    uint64_t CreateSharedMemory(uint64_t size, uint64_t flags, pid_t owner, pid_t recipient){
        acquireLock(&lock);

        uint64_t key = NextKey();

        if(!key) return 0;

        uint64_t pgCount = (size + 0xFFF) >> 12;

        shared_mem_t* sMem = (shared_mem_t*)kmalloc(sizeof(shared_mem_t));
        table[key] = sMem;
        sMem->pgCount = pgCount;
        sMem->pages = (uint64_t*)kmalloc(sMem->pgCount * sizeof(uint64_t*));
        sMem->mapCount = 0;

        for(unsigned i = 0; i < sMem->pgCount; i++){
            sMem->pages[i] = Memory::AllocatePhysicalMemoryBlock();
        }

        sMem->flags = flags;
        sMem->owner = owner;
        sMem->recipient = recipient;
        
        releaseLock(&lock);

        return key;
    }

    void* MapSharedMemory(uint64_t key, process_t* proc, uint64_t hint){
        acquireLock(&lock);

        shared_mem_t* sMem = GetSharedMemory(key);

        if(!sMem) {
            releaseLock(&lock);
            return nullptr; // Check for invalid key
        }

        if(sMem->flags & SMEM_FLAGS_PRIVATE){ // Private Mapping
            if(proc->pid != sMem->owner && proc->pid != sMem->recipient){
                releaseLock(&lock);
                return nullptr; // Does not have access rights
            }
        }
        sMem->mapCount++;

        void* mapping;
        
        if(hint && Memory::CheckRegion(hint, sMem->pgCount * PAGE_SIZE_4K, proc->addressSpace)){
            mapping = (void*)hint;
        } else mapping = Memory::Allocate4KPages(sMem->pgCount, proc->addressSpace);

        for(unsigned i = 0; i < sMem->pgCount; i++){
            Memory::MapVirtualMemory4K(sMem->pages[i], (uintptr_t)mapping + i * PAGE_SIZE_4K, 1, proc->addressSpace);
        }

        mem_region_t mReg;
        mReg.base = (uintptr_t)mapping;
        mReg.pageCount = sMem->pgCount;
        proc->sharedMemory.add_back(mReg);
        
        releaseLock(&lock);

        return mapping;
    }

    void DestroySharedMemory(uint64_t key){
        //acquireLock(&lock);
        
        shared_mem_t* sMem = GetSharedMemory(key);
        
        if(!sMem) {
            releaseLock(&lock);
            return; // Check for invalid key
        }

        if(sMem->mapCount > 0){
            Log::Error("Will not destroy active shared memory");
            return;
        }

        for(unsigned i = 0; i < sMem->pgCount; i++){
            Memory::FreePhysicalMemoryBlock(sMem->pages[i]);
        }

        table[key] = nullptr;

        kfree(sMem->pages); // Free physical page array
        kfree(sMem); // Free structure

        //releaseLock(&lock);
    }
}