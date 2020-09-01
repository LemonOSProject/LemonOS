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

#define THREAD_TIMESLICE_DEFAULT 7

enum {
	ThreadStateRunning,
	ThreadStateBlocked,
};

struct process;
struct thread;

typedef void* handle_t;
typedef uint64_t pid_t;

typedef struct HandleIndex {
	uint32_t owner_pid;
	process* owner;
	handle_t handle;
} handle_index_t;

typedef struct thread {
	lock_t lock = 0; // Thread lock
	lock_t stateLock = 0; // Thread state lock

	process* parent; // Parent Process
	void* stack; // Pointer to the initial stack
	void* stackLimit; // The limit of the stack
	void* kernelStack; // Kernel Stack
	uint32_t timeSlice;
	uint32_t timeSliceDefault;
	regs64_t registers;  // Registers
	void* fxState; // State of the extended registers

	thread* next; // Next thread in queue
	thread* prev; // Previous thread in queue
	
	uint8_t priority; // Thread priority
	uint8_t state; // Thread state

	uint64_t fsBase;

	List<List<thread*>*> waiting; // Thread is waiting in these queues
} thread_t;

typedef struct {
	uint64_t senderPID; // PID of Sender
	uint64_t recieverPID; // PID of Reciever
	uint64_t msg; // ID of message
	uint64_t data; // Message Data
	uint64_t data2;
} message_t;

namespace Scheduler{
	class ThreadBlocker{
		public:
		virtual void Block(thread_t* thread) = 0;
		virtual void Remove(thread_t* thread) = 0;

		virtual ~ThreadBlocker() = default;
	};

	class GenericThreadBlocker : public ThreadBlocker{
		public:
		List<thread_t*> blocked;
		
		public:
		void Block(thread_t* th) final {
			assert(th);
			blocked.add_back(th);
		}

		void Remove(thread_t* th) final {
			assert(th);
			blocked.remove(th);
		}
	};

	using FutexThreadBlocker = GenericThreadBlocker;
}

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

	Vector<fs_fd_t*> fileDescriptors;
	List<message_t> messageQueue;
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

	process_t* GetCurrentProcess();

	void Yield();

	handle_t RegisterHandle(void* handle);
	void* FindHandle(handle_t handle);

	int SendMessage(message_t msg);
	int SendMessage(process_t* proc, message_t msg);
	
	message_t RecieveMessage(process_t* proc);

	process_t* FindProcessByPID(uint64_t pid);
    uint64_t GetNextProccessPID(uint64_t pid);
	void InsertNewThreadIntoQueue(thread_t* thread);

	void BlockCurrentThread(ThreadBlocker& blocker, lock_t& lock);
	void BlockCurrentThread(List<thread_t*>& list, lock_t& lock);
	void UnblockThread(thread_t* thread);

    void Initialize();
    void Tick(regs64_t* r);

	void EndProcess(process_t* process);
}