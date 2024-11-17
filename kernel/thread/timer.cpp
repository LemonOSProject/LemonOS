#include <thread/timer.h>
#include <thread/lock.h>

#include "logging.h"

TimerQueue::TimerQueue(ClockDevice *clk) : m_clk(clk) {
    m_timer_cb = [this]() {
        auto current_time = m_clk->ns_since_boot();

        thread::LockGuard guard{ m_lock };
        while (!m_timers.empty() && m_timers.peek_priority() <= current_time) {
            auto timer = m_timers.pop_nofree();

            // Allow timer callbacks to add new timers
            guard.unlock();
            timer.call();
            guard.lock();
        }

        if (!m_timers.empty()) {
            auto next_timer = m_timers.peek_priority();
            m_clk->start_timer(next_timer);
        } else {
            m_clk->stop_timer();
        }
    };

    m_clk->timer_callback = m_timer_cb.callback();
}

void *TimerQueue::add_timer(Callback<> fn, long nanos) {
    thread::LockGuard guard{ m_lock };

    long deadline = m_clk->ns_since_boot() + nanos;

    int r = m_timers.push(fn, deadline);
    if (r) {
        return nullptr;
    }

    if (m_timers.peek_priority() == deadline) {
        m_clk->start_timer(deadline);
    }

    return (void*)1;
}

void TimerQueue::remove_timer(void *timer) {
    assert(!"not implemented");
}
