#include <lemon/sharedmem.h>

#include <lemon/syscall.h>
#include <sys/types.h>

namespace Lemon {
    uint64_t CreateSharedMemory(uint64_t size, uint64_t flags, pid_t owner, pid_t recipient){
        uint64_t key;
        syscall(SYS_CREATE_SHARED_MEMORY, &key, size, flags, recipient, 0);
        return key;
    }

    void* MapSharedMemory(uint64_t key){
        void* ptr;
        syscall(SYS_MAP_SHARED_MEMORY, &ptr, key, 0, 0, 0);
        return ptr;
    }

    long DestroySharedMemory(uint64_t key){
        return syscall(SYS_DESTROY_SHARED_MEMORY, key, 0, 0, 0, 0);
    }
}