#pragma once

#include <stdint.h>
#include <Paging.h>
#include <System.h>
#include <Memory.h>
#include <MM/AddressSpace.h>
#include <List.h>
#include <Vector.h>
#include <Fs/Filesystem.h>
#include <Lock.h>
#include <Timer.h>
#include <Hash.h>
#include <CPU.h>

#include <Objects/Handle.h>

#include <Thread.h>

#define KERNEL_CS 0x08
#define KERNEL_SS 0x10
#define USER_CS 0x1B
#define USER_SS 0x23

typedef void* handle_t;

namespace Scheduler{
	class ProcessStateThreadBlocker;
}

typedef struct Process {
	pid_t pid = -1; // PID
	AddressSpace* addressSpace; // Pointer to page directory and tables
	uint8_t state = ThreadStateRunning; // Process state
	Vector<Thread*> threads;
	uint32_t threadCount = 0; // Amount of threads
	int32_t euid = 0; // Effective UID
	int32_t uid = 0;
	int32_t gid = 0;

	Process* parent = nullptr;
	List<Process*> children;

	char workingDir[PATH_MAX];
	char name[NAME_MAX];

	timeval creationTime; // When the process was created
	uint64_t activeTicks = 0; // How many ticks this process has been active

	ReadWriteLock processLock;

	lock_t handleLock = 0;
	Vector<Handle> handles;
	Vector<fs_fd_t*> fileDescriptors;
	List<Scheduler::ProcessStateThreadBlocker*> blocking; // Threads blocking awaiting a state change
	HashMap<uintptr_t, List<FutexThreadBlocker*>*> futexWaitQueue = HashMap<uintptr_t, List<FutexThreadBlocker*>*>(8);

	uintptr_t usedMemoryBlocks;

	ALWAYS_INLINE PageMap* GetPageMap() { return addressSpace->GetPageMap(); }
} process_t;

namespace Scheduler{
	class ProcessStateThreadBlocker : public ThreadBlocker {
	protected:
		int& pid;
		List<process_t*> waitingOn;

	public:
		ProcessStateThreadBlocker(int& pidRef) : pid(pidRef){

		}

		void WaitOn(process_t* process);

		inline void Unblock(){
			assert(!"ProcessStateThreadBlocker: Base Unblock() not allowed!");
		}

		inline void Unblock(process_t* process){
			pid = process->pid;
			shouldBlock = false;

			acquireLock(&lock);
			for(auto it = waitingOn.begin(); it != waitingOn.end(); it++){
				if(*it == process){
					waitingOn.remove(it);
					process->blocking.remove(this);

					break;
				}
			}

			if(thread){
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
	process_t* CreateELFProcess(void* elf, int argc = 0, char** argv = nullptr, int envc = 0, char** envp = nullptr, const char* execPath = nullptr);
	void StartProcess(process_t* proc);

	inline static process_t* GetCurrentProcess(){
        CPU* cpu = GetCPULocal();

        process_t* ret = nullptr;

        if(cpu->currentThread)
            ret = cpu->currentThread->parent;

        return ret;
    }
	
	inline static Thread* GetCurrentThread(){
		return GetCPULocal()->currentThread;
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
}