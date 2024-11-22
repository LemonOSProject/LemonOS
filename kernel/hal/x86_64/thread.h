#pragma once

#include <stdint.h>

namespace thread {

struct TCB;

}

namespace hal {

void sched_init();
void reschedule();
void init_thread_context(thread::TCB *tcb, uint64_t fn, uint64_t stack_top);
[[noreturn]] void context_switch();

}