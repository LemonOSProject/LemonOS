#include "thread.h"

#include <thread/thread.h>
#include <thread/sched.h>
#include <thread/timer.h>
#include <panic.h>

#include "asm_macros.h"
#include "cpu.h"
#include "idt.h"
#include "logging.h"

namespace hal {

[[noreturn]] void sched_timer_expired(void *data, cpu::InterruptFrame *frame);

void sched_init() {
    install_irq_handler(SCHEDULE_IRQ, {
        .data = nullptr,
        .fn = sched_timer_expired
    });

    cpu::current()->sched = new thread::Scheduler();
}

void reschedule() {
    asm volatile("int %0" :: "i"(SCHEDULE_IRQ));
}

[[noreturn]] void sched_timer_expired(void *data, cpu::InterruptFrame *frame) {
    assert(!cpu::interrupt_flag());

    cpu::local_apic_eoi();

    auto *cpu = cpu::current();
    auto *thread = cpu->current_thread;
    auto *clk = get_best_clock_device();

    if (thread) {
        thread->saved_frame = *frame;
    }

    cpu->sched->run();

    auto *task = cpu->sched->current();
    if (!task) {
        lemon_panic("No tasks to run");
    }

    if (task->thread) {
        cpu->current_thread = task->thread;
    } else {
        lemon_panic("No thread for task");
    }

    auto &timer_queue = ::global_timer_queue();

    assert(task->time_slice_left > 0);

    assert(!cpu::interrupt_flag());
    timer_queue.add_timer(
        {
            .data = nullptr,
            .fn = [](void *data) {
                cpu::send_local_irq(SCHEDULE_IRQ, 0);
            }
        }, task->time_slice_left
    );
    assert(!cpu::interrupt_flag());

    context_switch();
}

void init_thread_context(thread::TCB *tcb, uint64_t fn, uint64_t stack_top) {
    tcb->saved_frame.r15 = 0;
    tcb->saved_frame.r14 = 0;
    tcb->saved_frame.r13 = 0;
    tcb->saved_frame.r12 = 0;
    tcb->saved_frame.r11 = 0;
    tcb->saved_frame.r10 = 0;
    tcb->saved_frame.r9 = 0;
    tcb->saved_frame.r8 = 0;
    tcb->saved_frame.rdi = 0;
    tcb->saved_frame.rsi = 0;
    tcb->saved_frame.rbp = 0;
    tcb->saved_frame.rdx = 0;
    tcb->saved_frame.rcx = 0;
    tcb->saved_frame.rbx = 0;
    tcb->saved_frame.rax = 0;
    tcb->saved_frame.err_code = 0;
    tcb->saved_frame.rip = (uint64_t)fn;
    tcb->saved_frame.cs = KERNEL_CS;

    // RFLAGS = IF
    tcb->saved_frame.rflags = 0x202;

    tcb->saved_frame.rsp = stack_top;
    tcb->saved_frame.ss = KERNEL_SS;
}

[[noreturn]] void context_switch() {
    auto *frame = &cpu::current()->current_thread->saved_frame;

    asm volatile(
        "mov %0, %%rsp\n"
        :: "r"(frame));

    asm volatile(
        POP_INTERRUPT_FRAME
        "iretq\n");
}

}