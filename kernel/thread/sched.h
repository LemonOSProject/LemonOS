#pragma once

#include <thread/thread.h>
#include <thread/lock.h>
#include <clock.h>

namespace thread {

class Scheduler {
public:
    void schedule_task(Task *task);
    void remove_task(Task *task);

    Task *current() {
        LockGuard guard{m_sched_lock};
        return m_current_task;
    }

    void run();

    void invalidate_current_task() {
        LockGuard guard{m_sched_lock};
        m_current_task = nullptr;
    }

private:
    void schedule_task_unlocked(Task *task);

    void debug_print_run_queue();

    TicketLock m_sched_lock;

    Task *m_run_queue_head = nullptr;
    Task *m_current_task = nullptr;

    ClockDevice *m_clk = get_best_clock_device();
};

}