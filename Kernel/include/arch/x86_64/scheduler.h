#pragma once

#include <stdint.h>
#include <paging.h>
#include <system.h>
#include <memory.h>
#include <list.h>
#include <vector.h>
#include <fs/filesystem.h>
#include <lock.h>
#include <timer.h>
#include <hash.h>
#include <cpu.h>

#include <objects/handle.h>

#include <thread.h>

typedef void* handle_t;

typedef struct HandleIndex {
	uint32_t owner_pid;
	process* owner;
	handle_t handle;
} handle_index_t;

typedef struct process {
	pid_t pid = -1; // PID
	address_space_t* addressSpace; // Pointer to page directory and tables
	List<mem_region_t> sharedMemory; // Used to ensure these memory regions don't get freed when a process is terminated
	uint8_t state = ThreadStateRunning; // Process state
	Vector<thread_t*> threads;
	uint32_t threadCount = 0; // Amount of threads
	int32_t uid = 0;
	int32_t gid = 0;

	process* parent = nullptr;
	List<process*> children;

	char workingDir[PATH_MAX];
	char name[NAME_MAX];

	timeval_t creationTime; // When the process was created
	uint64_t activeTicks = 0; // How many ticks this process has been active

	lock_t handleLock = 0;
	Vector<Handle> handles;
	Vector<fs_fd_t*> fileDescriptors;
	List<thread_t*> blocking; // Threads blocking awaiting a state change
	HashMap<uintptr_t, Scheduler::FutexThreadBlocker*> futexWaitQueue;
} process_t;

typedef struct {
	pid_t pid; // Process ID

	uint32_t threadCount; // Process Thread Count

	int32_t uid; // User ID
	int32_t gid; // Group ID

	uint8_t state; // Process State

	char name[NAME_MAX]; // Process Name

	uint64_t runningTime; // Amount of time in seconds that the process has been running
	uint64_t activeUs;
} process_info_t;

namespace Scheduler{
    pid_t CreateChildThread(process_t* process, uintptr_t entry, uintptr_t stack);

    process_t* CreateProcess(void* entry);
	process_t* CreateELFProcess(void* elf, int argc = 0, char** argv = nullptr, int envc = 0, char** envp = nullptr);

	inline static process_t* GetCurrentProcess(){
        asm("cli");
        CPU* cpu = GetCPULocal();

        process_t* ret = nullptr;

        if(cpu->currentThread)
            ret = cpu->currentThread->parent;

        asm("sti");
        return ret;
    }

	Handle& RegisterHandle(process_t* proc, FancyRefPtr<KernelObject> ko);
	long FindHandle(process_t* proc, handle_id_t id, Handle& ref);
	long DestroyHandle(process_t* proc, handle_id_t id);

	void Yield();

	process_t* FindProcessByPID(uint64_t pid);
    uint64_t GetNextProccessPID(uint64_t pid);
	void InsertNewThreadIntoQueue(thread_t* thread);

    void Initialize();
    void Tick(regs64_t* r);

	void EndProcess(process_t* process);
}