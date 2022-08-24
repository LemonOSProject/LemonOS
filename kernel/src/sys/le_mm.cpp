#include "syscall.h"

#include <MM/AddressSpace.h>
#include <Objects/Process.h>
#include <Errno.h>
#include <Logging.h>

#include <abi-bits/vm-flags.h>

long sys_mmap(void* address, size_t len, long flags, le_handle_t handle, long off, UserPointer<void*> returnAddress) {
    if(!len) {
        // length of zero is not allowed
        return EINVAL;
    }

    if(((uintptr_t)address) & (PAGE_SIZE_4K - 1)) {
        return EINVAL; // Address is not aligned to page boundary
    }

    int prot = flags >> 32;

    // Either MAP_PRIVATE OR MAP_SHARED must be specified
    bool shared = (flags & MAP_SHARED) != 0;
    if(!shared && !(flags & MAP_PRIVATE)) {
        Log::Warning("sys_mmap: MAP_PRIVATE OR MAP_SHARED must be specified");
        return EINVAL;
    }

    bool fixed = flags & MAP_FIXED;
    bool anon = flags & MAP_ANON;

    // MAP_ANON must use MAP_PRIVATE (for now)
    if(anon && shared) {
        Log::Warning("sys_mmap: Anonymous shared mappings not supported!");
        return EINVAL;
    }

    len = (len + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);

    Process* process = Process::Current();
    if(!anon) {
        auto file = FD_GET(handle);

        MappedRegion* region = SC_TRY_OR_ERROR(file->MMap((uintptr_t)address, len, off, prot, shared, fixed));
        assert(region);

        if(returnAddress.StoreValue((void*)region->Base())) {
            return EFAULT;
        }

        return 0;
    }

    // Allocate anonymous memory
    MappedRegion* region = process->addressSpace->AllocateAnonymousVMObject(len, (uintptr_t)address, fixed);
    if(!region) {
        // Fixed region likely was taken
        return EEXIST;
    }

    if(returnAddress.StoreValue((void*)region->Base())) {
        return EFAULT;
    }

    return 0;
}

long sys_mprotect(void* address, size_t len, int prot) {
    return ENOSYS;
}

long sys_munmap(void* address, size_t len) {
    Process* process = Process::Current();

    // address and len must be multiples of page size
    if(((uintptr_t)address) & (PAGE_SIZE_4K - 1) || (len & (PAGE_SIZE_4K - 1))) {
        return EINVAL;
    }

    return process->addressSpace->UnmapMemory((uintptr_t)address, len);
}
