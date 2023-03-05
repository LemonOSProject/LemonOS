#pragma once

#include <abi/types.h>

#include <MM/VMObject.h>
#include <Scheduler.h>

#define SMEM_FLAGS_PRIVATE 1

class SharedVMObject : public PhysicalVMObject {
public:
    SharedVMObject(size_t size, int64_t key, pid_t owner, pid_t recipient, bool isPrivate);

    ALWAYS_INLINE int64_t Key() const { return key; }
    ALWAYS_INLINE pid_t Owner() const { return owner; }
    ALWAYS_INLINE pid_t Recipient() const { return recipient; }

    ALWAYS_INLINE bool IsPrivate() const { return isPrivate; }
    ALWAYS_INLINE bool CanMunmap() const override { return true; }
private:
    int64_t key; // Key

    pid_t owner; // Owner Process
    pid_t recipient; // Recipient Process (if private)

    bool isPrivate : 1 = false;
};

namespace Memory{
    int CanModifySharedMemory(pid_t pid, int64_t key);
    FancyRefPtr<SharedVMObject> GetSharedMemory(int64_t key);
    
    int64_t CreateSharedMemory(uint64_t size, uint64_t flags, pid_t owner, pid_t recipient);
    void* MapSharedMemory(int64_t key, Process* proc, uint64_t hint);
    void DestroySharedMemory(int64_t key);
}