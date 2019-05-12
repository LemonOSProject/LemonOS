#pragma once

#include <stdint.h>
#include <paging.h>
#include <system.h>
#include <list.h>
#include <filesystem.h>

#define PROCESS_STATE_SUSPENDED 0
#define PROCESS_STATE_ACTIVE 1
struct process;

typedef void* handle_t;

typedef struct HandleIndex {
	uint32_t owner_pid;
	process* owner;
	handle_t handle;
} handle_index_t;

typedef struct {
	process* parent; // Parent Process
	void* stack; // Pointer to the initial stack
	void* stackLimit; // The limit of the stack
	uint8_t priority; // Thread priority
	uint8_t state; // Thread state
	regs32_t registers;  // Registers
} __attribute__((packed)) thread_t;

typedef struct {
	uint32_t senderPID; // PID of Sender
	uint32_t recieverPID; // PID of Reciever
	uint32_t id; // ID of message
	uint32_t data; // Message Data
} __attribute__((packed)) message_t;

typedef struct process {
	uint64_t pid; // PID
	uint8_t priority; // Process Priority
	page_directory_ptr_t pageDirectory; // Pointer to page directory and tables
	uint8_t state; // Process state
	//thread_t* threads; // Array of threads
	thread_t threads[1];
	uint32_t thread_count; // Amount of threads
	process* next;

	List<fs_node_t*> fileDescriptors;
	List<message_t> messageQueue;
} process_t;
/*
void register_handle(handle_t handle);
uint32_t create_process(void* entry);
uint32_t create_process_elf(void* elf);
void add_thread(uint32_t pid, void * entry);
//void scheduler_tick(regs32_t *r);
void multitasking_init();
process_t* find_by_pid(uint32_t pid);
process_t* get_current_proc();
handle_index_t find_handle(handle_t handle);
*/
namespace Scheduler{
    uint64_t CreateProcess(void* entry);
    uint64_t LoadELF(void* entry);

	process_t* GetCurrentProcess();

	void RegisterHandle(handle_t handle);
	handle_index_t FindHandle(handle_t handle);

	int SendMessage(message_t msg);
	int SendMessage(process_t* proc, message_t msg);
	
	message_t RecieveMessage(process_t* proc);

    void Initialize();
    void Tick();
}