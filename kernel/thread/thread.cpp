#include <thread/thread.h>

#include <thread/sched.h>
#include <thread.h>
#include <vmem.h>
#include <mm/address_space.h>

#include "cpu.h"

namespace thread {

void TCB::kill() {
    hal::cpu::InterruptDisabler disable_ints;

    // TODO: cleanup stack etc.
    if (current() == this) {
        delete this;

        auto *cpu = hal::cpu::current();
        cpu->current_thread = nullptr;

        cpu->sched->invalidate_current_task();

        hal::reschedule();

        lemon_panic("Failed to kill thread");
    } else {
        lemon_panic("TCB::kill() called by another thread");
    }
}

TCB *create_kernel_thread(const char *tag, void fn(void), size_t stack_sz, task_priority_t priority) {
    hal::cpu::InterruptDisabler disable_ints;

    auto *tcb = new TCB();

    auto *aspace = hal::kernel_address_space;
    
    mm::MemoryRegion *region = new mm::MemoryRegion;

    int r = aspace->allocate_range_for_region(region, stack_sz, { .read = true, .write = true, .execute = false });
    assert(!r);

    auto pg_flags = hal::get_page_flags_for_prot({ .read = true, .write = true, .execute = false });

    log_info("stack region: {:x}-{:x}", region->base, region->end());
    log_info("stack top: {:x}", (uint8_t*)region->base + stack_sz);

    for (int i = 0; i < NUM_PAGES_4K(stack_sz); i++) {
        auto *frame = mm::alloc_frame();
        assert(frame);

        hal::kernel_page_map->page_range_map(
            region->base + i * PAGE_SIZE_4K, frame->get_address(), pg_flags, 1);
    }

    tcb->kernel_stack = (uint8_t*)region->base + stack_sz;
    tcb->tid = 0;
    tcb->task.thread = tcb;
    tcb->task.real_priority = priority;
    tcb->task.effective_priority = priority;
    tcb->task.time_slice = 5'000'000; // 5ms
    tcb->task.time_slice_left = 0;
    tcb->task.last_ran = 0;
    tcb->task.wait_queue_next = nullptr;
    tcb->task.wait_queue_prev = nullptr;

    tcb->tag = tag;

    // Set up the stack
    auto stack = tcb->kernel_stack;

    hal::init_thread_context(tcb, (uint64_t)fn, (uint64_t)stack);
    
    hal::cpu::current()->sched->schedule_task(&tcb->task);

    return tcb;
}

}