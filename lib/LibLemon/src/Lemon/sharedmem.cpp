#include <Lemon/Core/SharedMemory.h>

#include <lemon/syscall.h>
#include <sys/types.h>

namespace Lemon {
int64_t CreateSharedMemory(uint64_t size, uint64_t flags) {
    int64_t key;
    long ret = syscall(SYS_CREATE_SHARED_MEMORY, &key, size, flags, /*recipient*/ 0);
    if (ret < 0) {
        return ret;
    }
    return key;
}

void* MapSharedMemory(int64_t key) {
    volatile void* ptr;
    syscall(SYS_MAP_SHARED_MEMORY, &ptr, key, 0);
    return (void*)ptr;
}

long UnmapSharedMemory(void* address, int64_t key) { return syscall(SYS_UNMAP_SHARED_MEMORY, address, key); }

long DestroySharedMemory(int64_t key) { return syscall(SYS_DESTROY_SHARED_MEMORY, key); }
} // namespace Lemon
