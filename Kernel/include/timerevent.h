#pragma once

#include <spin.h>
#include <list.h>

struct RegisterContext;

namespace Timer{
    using TimerCallback = void(*)(void*);
    void Handler(void*, RegisterContext* r);

    class TimerEvent final {
        friend void Timer::Handler(void*, RegisterContext* r);
        friend class ::FastList<TimerEvent*>;
    protected:
        long ticks = 0;
        bool dispatched = false;

        lock_t lock = 0;

        TimerEvent* next = nullptr;
        TimerEvent* prev = nullptr;

        TimerCallback callback;
        void* data = nullptr; // Generic data pointer (Could be used to point to a class, etc.)

        inline void Dispatch() {
            dispatched = true;
            callback(data);
        }
    public:
        TimerEvent(long _us, TimerCallback _callback, void* data);
        ~TimerEvent();

        inline long GetTicks() const { return ticks; }

        __attribute__((always_inline)) inline void Lock() { acquireLock(&lock); }
        __attribute__((always_inline)) inline void Unlock() { releaseLock(&lock); }
    };
}