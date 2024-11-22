#include <thread/sched.h>

#include <logging.h>
#include <clock.h>

namespace thread {

void Scheduler::schedule_task(Task *task) {
    LockGuard guard{m_sched_lock};
    
    schedule_task_unlocked(task);
}

void Scheduler::run() {
    LockGuard guard{m_sched_lock};

    auto *current = m_current_task;
    if (!current) {
        assert(m_run_queue_head);

        // Get the first task
        m_current_task = m_run_queue_head;
        m_run_queue_head = m_current_task->remove();
        m_current_task->last_ran = m_clk->ns_since_boot();
        m_current_task->time_slice_left = m_current_task->time_slice;

        //log_info("running task: time_slice: {}, last_ran: {}, time_slice_left: {}", m_current_task->time_slice, m_current_task->last_ran, m_current_task->time_slice_left);
        return;
    }

    // Check if the current task has run out of time slice
    current->time_slice_left = m_current_task->time_slice_left
        - (m_clk->ns_since_boot() - current->last_ran);

    //log_info("running task: time_slice: {}, last_ran: {}, time_slice_left: {}", m_current_task->time_slice, m_current_task->last_ran, m_current_task->time_slice_left);

    if (current->time_slice_left > 0) {
        return;
    }

    // Put the current task back to the queue
    schedule_task_unlocked(current);

    // Get the next task
    m_current_task = m_run_queue_head;
    m_run_queue_head = m_current_task->remove();
    m_current_task->last_ran = m_clk->ns_since_boot();
    m_current_task->time_slice_left = m_current_task->time_slice;
}

void Scheduler::schedule_task_unlocked(Task *task) {
    if (!m_run_queue_head) {
        m_run_queue_head = task;
        task->create_list();
    } else {
        // Sort by lowest priority first,
        // then by last ran time

        // Using a list is far from effective, but oh well

        // First check the head
        Task *node = m_run_queue_head;
        if (node->effective_priority > task->effective_priority
                || (node->effective_priority == task->effective_priority && node->last_ran > task->last_ran)) {
            m_run_queue_head = node->insert_before(task);
            return;
        }

        // Find the first task where priority <= task->priority
        while (node->get_next() && node->get_next()->effective_priority < task->effective_priority) {
            node = node->get_next();
        }

        // Find the first task where last_ran >= task->last_ran
        while (node->get_next() && node->get_next()->last_ran < task->last_ran && node->get_next()->effective_priority == task->effective_priority) {
            node = node->get_next();
        }

        // Insert the task
        if (node->get_next()) {
            node->insert_after(task);
        } else {
            node->push_back(task);
        }
    }
}

void Scheduler::debug_print_run_queue() {
    auto *node = m_run_queue_head;
    while (node && node != m_run_queue_head) {
        log_info("{} ->", node->thread->tag);
        node = node->get_next();
    }
}

}