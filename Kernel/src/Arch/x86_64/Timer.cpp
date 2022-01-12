#include <Timer.h>
#include <TimerEvent.h>

#include <MiscHdr.h>

#include <APIC.h>
#include <CPU.h>
#include <IDT.h>
#include <List.h>
#include <Logging.h>
#include <Scheduler.h>
#include <IOPorts.h>

namespace Timer {
int frequency;         // Timer frequency
long ticks = 0;        // Timer tick counter
long pendingTicks = 0; // If the sleep queue is locked add ticks here
long long uptime = 0;  // System uptime in seconds since the timer was initialized

lock_t sleepQueueLock = 0;

// In the sleep queue, all waiting threads have a counter as an offset from the previous waiting thread.
// For example there are 2 threads, thread 1 is waiting for 10 ticks and thread 2 is waiting for 15 ticks.
// Thread 1's ticks value will be 10, and as 15 - 10 is 5, thread 2's value will be 5 so it waits 5 ticks after thread 1
FastList<TimerEvent*> sleeping;

TimerEvent::TimerEvent(long _us, void (*_callback)(void*), void* _data)
    : ticks(_us * frequency / 1000000), callback(_callback), data(_data) {
    if (ticks <= 0) {
        callback(data);
        return;
    }

    acquireLock(&sleepQueueLock);

    TimerEvent* ev = sleeping.get_front();
    while (ev) {
        if (ev->ticks >= ticks) { // See if we need to insert in middle of queue
            ev->ticks -= ticks;
            sleeping.insert(this, ev); // Insert before

            releaseLock(&sleepQueueLock);
            return;
        }

        ticks -= ev->ticks;
        ev = ev->next;

        if (ev == sleeping.get_front()) {
            break;
        }
    }

    sleeping.add_back(this); // Add to back
    releaseLock(&sleepQueueLock);
}

TimerEvent::~TimerEvent() {
    acquireLock(&sleepQueueLock);
    acquireLock(&lock);

    if (!dispatched) {
        dispatched = true;

        if (next) {
            if (next && ticks >= 0 && next != sleeping.get_front()) { // Make sure not at end of list
                next->ticks += ticks;
            }

            sleeping.remove(this); // Remove from queue
        }
    }

    releaseLock(&sleepQueueLock);
    releaseLock(&lock);
}

void TimerEvent::Dispatch() {
    acquireLock(&lock);
    if (!dispatched) {
        dispatched = true;
        sleeping.remove(this);

        callback(data);
    }
    releaseLock(&lock);
}

uint64_t GetSystemUptime() { return uptime; }

uint32_t GetTicks() { return ticks; }

uint32_t GetFrequency() { return frequency; }

inline uint64_t UsToTicks(long us) { return us * frequency / 1000000; }

timeval GetSystemUptimeStruct() {
    timeval tval;
    tval.tv_sec = uptime;
    tval.tv_usec = ticks * 1000000 / frequency;
    return tval;
}

long TimeDifference(const timeval& newTime, const timeval& oldTime) {
    long seconds = newTime.tv_sec - oldTime.tv_sec;
    int microseconds = newTime.tv_usec - oldTime.tv_usec;

    return seconds * 1000000 + microseconds;
}

void SleepCurrentThread(timeval& time) { Scheduler::GetCurrentThread()->Sleep(time.tv_sec * 1000000 + time.tv_usec); }

void Wait(long ms) {
    assert(ms > 0);

    uint64_t timeMs = uptime * 1000 + (ticks * frequency / 1000);
    while ((uptime * 1000 + (ticks * frequency / 1000)) - timeMs <= static_cast<unsigned long>(ms))
        ;
}

// Timer handler
void Handler(void*, RegisterContext* r) {
    ticks++;
    if (ticks >= frequency) {
        uptime++;
        ticks -= frequency;
    }

    pendingTicks++;
    if (!(acquireTestLock(&sleepQueueLock))) {
        while (sleeping.get_length() &&
               pendingTicks-- >
                   0) { // Make sure the queue has not changed inbetween checking the length and acquiring the lock
            TimerEvent* cnt = sleeping.get_front();

            assert(cnt);
            cnt->ticks--;

            if (cnt->ticks <= 0) {
                sleeping.get_front()->Dispatch();

                while (sleeping.get_length() && sleeping.get_front()->ticks <= 0) {
                    sleeping.get_front()->Dispatch();
                }
            }
        }
        pendingTicks = 0;

        releaseLock(&sleepQueueLock);
    }

    Scheduler::Tick(r);
}

// Initialize
void Initialize(uint32_t freq) {
    IDT::RegisterInterruptHandler(IRQ0, Handler);

    frequency = freq;
    uint32_t divisor = 1193182 / freq;

    // Send the command byte.
    outportb(0x43, 0x36);

    // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
    uint8_t l = divisor & 0xff;
    uint8_t h = divisor >> 8;

    // Send the frequency divisor.
    outportb(0x40, l);
    outportb(0x40, h);
}
} // namespace Timer
