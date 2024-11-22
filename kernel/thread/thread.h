#pragma once

#include "cpu.h"

#include <le/intrusive_list.h>
#include <le/fn.h>
#include <stddef.h>

namespace thread {

typedef uint8_t task_priority_t;
typedef int64_t tid_t;

struct RunQueue{};
struct WaitQueue{};

// Separate to a thread, as a thread's Task structure
// may be inherited by another thread
struct Task : public IntrusiveListNode<Task, RunQueue> {
    // Real priority of the thread
    task_priority_t real_priority;
    // Effective priority of the thread,
    // may change if a thread of a higher priority is waiting for a resource held by this thread
    task_priority_t effective_priority;

    int64_t time_slice;
    int64_t time_slice_left;

    int64_t last_ran;

    // Used to link threads waiting for a resource
    Task *wait_queue_next;
    Task *wait_queue_prev;

    struct TCB *thread;
};

struct TCB {
    tid_t tid;
    hal::cpu::InterruptFrame saved_frame;

    void *kernel_stack;
    
    Task task;

    const char *tag;

    void kill();
};

TCB *create_kernel_thread(const char *tag, void fn(void), size_t stack_sz, task_priority_t priority);
static inline TCB *current() {
    hal::cpu::InterruptDisabler disable_ints;
    return hal::cpu::current()->current_thread;
}

};
