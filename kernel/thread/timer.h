#pragma once

#include <cpu.h>
#include <stddef.h>

#include <le/pq.h>
#include <le/fn.h>
#include <le/callback.h>

#include <thread/lock.h>

#include <clock.h>

class TimerQueue {
public:
    TimerQueue(ClockDevice *clk);

    void *add_timer(Callback<> fn, long nanos);
    void remove_timer(void *timer);

private:
    PQ<Callback<>, long> m_timers;

    ClockDevice *m_clk;
    thread::TicketLock m_lock;
    Fn<> m_timer_cb;
};

namespace hal {

extern TimerQueue *global_timer_queue;

};

inline TimerQueue &global_timer_queue() {
    return *hal::global_timer_queue;
}
