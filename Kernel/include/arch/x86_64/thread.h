#pragma once

#include <spin.h>
#include <list.h>

#define THREAD_TIMESLICE_DEFAULT 7
typedef uint64_t pid_t;

enum {
	ThreadStateRunning,
	ThreadStateBlocked,
	ThreadStateZombie,
};

struct process;
struct thread;

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

	void BlockCurrentThread(ThreadBlocker& blocker);
	void BlockCurrentThread(List<thread_t*>& list);
	void BlockCurrentThread(ThreadBlocker& blocker, lock_t& lock);
	void BlockCurrentThread(List<thread_t*>& list, lock_t& lock);
	void UnblockThread(thread_t* thread);
}