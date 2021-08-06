#pragma once

#include <CPU.h>
#include <ELF.h>
#include <Fs/Filesystem.h>
#include <Hash.h>
#include <List.h>
#include <Lock.h>
#include <MM/AddressSpace.h>
#include <Memory.h>
#include <Paging.h>
#include <System.h>
#include <Timer.h>
#include <Vector.h>
#include <stdint.h>

#include <Objects/Handle.h>

#include <Thread.h>

#define KERNEL_CS 0x08
#define KERNEL_SS 0x10
#define USER_CS 0x1B
#define USER_SS 0x23

typedef void* handle_t;

namespace Scheduler {
class ProcessStateThreadBlocker;
}

typedef struct Process {
    pid_t pid = -1;             // PID
    AddressSpace* addressSpace; // Pointer to page directory and tables

    List<Thread*> threads;
    int64_t nextThreadID = 1;

    int32_t euid = 0; // Effective UID
    int32_t uid = 0;
    int32_t egid = 0; // Effective GID
    int32_t gid = 0;

    bool isDying = false;
    bool isDead = false;
    Process* parent = nullptr;
    List<Process*> children;

    char workingDir[PATH_MAX];
    char name[NAME_MAX];

    MappedRegion* signalTrampoline;
    SignalHandler signalHandlers[SIGNAL_MAX]; // Signal handlers
    siginfo_t* siginfoPointer = nullptr;

    timeval creationTime;     // When the process was created
    uint64_t activeTicks = 0; // How many ticks this process has been active

    ReadWriteLock processLock;

    lock_t handleLock = 0;
    Vector<Handle> handles;
    List<Scheduler::ProcessStateThreadBlocker*> blocking; // Threads blocking awaiting a state change
    HashMap<uintptr_t, List<FutexThreadBlocker*>*> futexWaitQueue = HashMap<uintptr_t, List<FutexThreadBlocker*>*>(8);

    uintptr_t usedMemoryBlocks;

    ALWAYS_INLINE Thread* GetThreadFromID(pid_t id) {
        for (Thread* t : threads) {
            if (t && t->tid == id) {
                return t;
            }
        }

        assert(id != 1);
        return nullptr;
    }

    ALWAYS_INLINE int AllocateFileDescriptor(fs_fd_t* ptr) {
        ScopedSpinLock lockFds(fileDescriptorsLock);

        unsigned i = 0;
        for (; i < fileDescriptors.get_length(); i++) {
            if (!fileDescriptors[i]) {
                fileDescriptors[i] = ptr;
                return i;
            }
        }

        fileDescriptors.add_back(ptr);
        return i;
    }

    ALWAYS_INLINE int ReplaceFileDescriptor(int fd, fs_fd_t* ptr) {
        ScopedSpinLock lockFds(fileDescriptorsLock);
        if (fd > fileDescriptors.get_length()) {
            return 1;
        } else if (fd == fileDescriptors.get_length()) {
            fileDescriptors.add_back(ptr);
            return 0;
        } else {
            fs_fd_t*& f = fileDescriptors[fd];
            if (f) {
                fs::Close(f);
                delete f;
            }

            f = ptr;
            return 0;
        }
    }

    ALWAYS_INLINE int DestroyFileDescriptor(int fd) {
        ScopedSpinLock lockFds(fileDescriptorsLock);
        if (fd >= fileDescriptors.get_length()) {
            return 1;
        }

        if (fs_fd_t*& f = fileDescriptors[fd]; f) {
            fs::Close(f);
            delete f;

            f = nullptr;

            return 0;
        }

        return 1;
    }

    ALWAYS_INLINE fs_fd_t* GetFileDescriptor(int fd) {
        ScopedSpinLock lockFds(fileDescriptorsLock);
        if (fd < fileDescriptors.get_length()) {
            return fileDescriptors[fd];
        }

        return nullptr;
    }

    ALWAYS_INLINE unsigned FileDescriptorCount() const { return fileDescriptors.get_length(); }

    ALWAYS_INLINE PageMap* GetPageMap() { return addressSpace->GetPageMap(); }

    ALWAYS_INLINE void RemoveChild(Process* child) {
        for (auto it = children.begin(); it != children.end(); it++) {
            Process* process = *it;
            if (process == child) {
                assert(process->isDead);   // Ensure the process is actually dead
                children.remove(it);       // Remove the process
                process->parent = nullptr; // Let the reaper thread know this process can die
                break;
            }
        }
    }

    ALWAYS_INLINE Process* FindChildByPID(pid_t pid) {
        for (Process* proc : children) {
            if (proc->pid == pid) {
                return proc;
            }
        }

        return nullptr;
    }

    lock_t fileDescriptorsLock = 0;
    Vector<fs_fd_t*> fileDescriptors;

private:
} process_t;

namespace Scheduler {
class ProcessStateThreadBlocker : public ThreadBlocker {
protected:
    int& pid;
    List<process_t*> waitingOn;

public:
    ProcessStateThreadBlocker(int& pidRef) : pid(pidRef) {}

    void WaitOn(process_t* process);

    inline void Unblock() { assert(!"ProcessStateThreadBlocker: Base Unblock() not allowed!"); }

    inline void Unblock(process_t* process) {
        pid = process->pid;
        shouldBlock = false;

        acquireLock(&lock);
        for (auto it = waitingOn.begin(); it != waitingOn.end(); it++) {
            if (*it == process) {
                waitingOn.remove(it);
                process->blocking.remove(this);

                break;
            }
        }

        if (thread) {
            thread->Unblock();
        }
        releaseLock(&lock);
    }

    ~ProcessStateThreadBlocker();
};

extern lock_t destroyedProcessesLock;
extern List<process_t*>* destroyedProcesses;

pid_t CreateChildThread(process_t* process, uintptr_t entry, uintptr_t stack, uint64_t cs, uint64_t ss);

process_t* CreateProcess(void* entry);
process_t* CloneProcess(process_t* process);

process_t* CreateELFProcess(void* elf, int argc = 0, char** argv = nullptr, int envc = 0, char** envp = nullptr,
                            const char* execPath = nullptr);
uintptr_t LoadELF(Process* process, uintptr_t* stack, elf_info_t elf, int argc = 0, char** argv = nullptr, int envc = 0,
                  char** envp = nullptr, const char* execPath = nullptr);

void StartProcess(process_t* proc);

inline static process_t* GetCurrentProcess() {
    CPU* cpu = GetCPULocal();

    process_t* ret = nullptr;

    if (cpu->currentThread)
        ret = cpu->currentThread->parent;

    return ret;
}

inline static Thread* GetCurrentThread() { return GetCPULocal()->currentThread; }

// Checks that a pointer of type T is valid
template <typename T>
inline bool CheckUsermodePointer(T* ptr, AddressSpace* addressSpace = GetCurrentProcess()->addressSpace) {
    return addressSpace->RangeInRegion(reinterpret_cast<uintptr_t>(ptr), sizeof(T));
}

Handle& RegisterHandle(process_t* proc, FancyRefPtr<KernelObject> ko);
long FindHandle(process_t* proc, handle_id_t id, Handle** ref);
long DestroyHandle(process_t* proc, handle_id_t id);

void Yield();
void Schedule(void* data, RegisterContext* r);

process_t* FindProcessByPID(pid_t pid);
pid_t GetNextProccessPID(pid_t pid);
void InsertNewThreadIntoQueue(Thread* thread);

void Initialize();
void Tick(RegisterContext* r);

void EndProcess(process_t* process);
} // namespace Scheduler