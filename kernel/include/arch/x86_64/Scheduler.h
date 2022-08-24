#pragma once

#include <CPU.h>
#include <ELF.h>
#include <Fs/Filesystem.h>
#include <Hash.h>
#include <List.h>
#include <Lock.h>
#include <MM/AddressSpace.h>
#include <Memory.h>
#include <Objects/Handle.h>
#include <Objects/Process.h>
#include <Paging.h>
#include <Thread.h>
#include <Timer.h>
#include <Vector.h>
#include <stdint.h>

#define KERNEL_CS 0x08
#define KERNEL_SS 0x10
#define USER_SS 0x1B
#define USER_CS 0x23

namespace Scheduler {
class ProcessStateThreadBlocker;
}

namespace Scheduler {
extern lock_t processesLock;
extern lock_t destroyedProcessesLock;
extern List<FancyRefPtr<Process>>* destroyedProcesses;

void RegisterProcess(FancyRefPtr<Process> proc);
void MarkProcessForDestruction(Process* proc);

ALWAYS_INLINE static Process* GetCurrentProcess() {
    return Thread::Current()->parent;
}

// Checks that a pointer of type T is valid
template <typename T>
ALWAYS_INLINE bool CheckUsermodePointer(T* ptr, AddressSpace* addressSpace = GetCurrentProcess()->addressSpace) {
    return addressSpace->RangeInRegion(reinterpret_cast<uintptr_t>(ptr), sizeof(T));
}

void Yield();
void Schedule(void* data, RegisterContext* r);
void DoSwitch(CPU* cpu);

pid_t GetNextPID();
FancyRefPtr<Process> FindProcessByPID(pid_t pid);
pid_t GetNextProcessPID(pid_t pid);
void InsertNewThreadIntoQueue(Thread* thread);
void BalanceRunQueues();

void Initialize();
void Tick(RegisterContext* r);
} // namespace Scheduler