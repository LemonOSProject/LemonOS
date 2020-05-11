#pragma once

#include <stdint.h>
#include <paging.h>
#include <system.h>
#include <list.h>
#include <filesystem.h>
#include <memory.h>

#define PROCESS_STATE_SUSPENDED 0
#define PROCESS_STATE_ACTIVE 1
struct process;

typedef void* handle_t;
typedef uint64_t pid_t;

typedef struct HandleIndex {
	uint32_t owner_pid;
	process* owner;
	handle_t handle;
} handle_index_t;

typedef struct {
	process* parent; // Parent Process
	void* stack; // Pointer to the initial stack
	void* stackLimit; // The limit of the stack
	void* kernelStack; // Kernel Stack
	uint8_t priority; // Thread priority
	uint8_t state; // Thread state
	regs64_t registers;  // Registers
} __attribute__((packed)) thread_t;

typedef struct {
	uint64_t senderPID; // PID of Sender
	uint64_t recieverPID; // PID of Reciever
	uint64_t msg; // ID of message
	uint64_t data; // Message Data
	uint64_t data2;
} __attribute__((packed)) message_t;

typedef struct process {
	pid_t pid; // PID
	uint8_t priority; // Process Priority
	address_space_t* addressSpace; // Pointer to page directory and tables
	List<mem_region_t> sharedMemory; // Used to ensure these memory regions don't get freed when a process is terminated
	uint8_t state; // Process state
	//thread_t* threads; // Array of threads
	thread_t threads[1];
	uint32_t thread_count; // Amount of threads
	uint32_t timeSlice;
	uint32_t timeSliceDefault;
	process* next;

	process* parent;
	List<process*> children;

	char workingDir[PATH_MAX];

	void* fxState; // State of the extended registers

	List<fs_fd_t*> fileDescriptors;
	List<message_t> messageQueue;
} process_t;

namespace Scheduler{
	extern process_t* currentProcess;
	extern int lock;

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

    void Initialize();
    void Tick(regs64_t* r);

	void EndProcess(process_t* process);
}