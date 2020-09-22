#include <timer.h>

#include <idt.h>
#include <scheduler.h>
#include <system.h>
#include <apic.h>
#include <list.h>
#include <cpu.h>
#include <logging.h>

namespace Timer{

    int frequency; // Timer frequency
    int ticks = 0; // Timer tick counter
    long long uptime = 0; // System uptime in seconds since the timer was initialized

    struct SleepCounter{
        thread_t* thread;
        long ticksLeft;
    };

    lock_t sleepQueueLock = 0; // Prevent deadlocks

    // In the sleep queue, all waiting threads have a counter as an offset from the previous waiting thread.
    // For example there are 2 threads, thread 1 is waiting for 10 ticks and thread 2 is waiting for 15 ticks.
    // Thread 1's ticks value will be 10, and as 15 - 10 is 5, thread 2's value will be 5 so it waits 5 ticks after thread 1
    List<SleepCounter> sleeping;

    class SleepBlocker : public Scheduler::ThreadBlocker {
        private:
            long ticks = 0;
        public:
        SleepBlocker(long ticks){
            this->ticks = ticks;
        }

        void Block(thread_t* thread) final {
            if(!sleeping.get_length()){
                sleeping.add_back({.thread = thread, .ticksLeft = ticks}); // No sleeping threads in queue, just add
                return;
            }

            long total = 0;
            for(unsigned i = 0; i < sleeping.get_length(); i++){
                if(total + sleeping[i].ticksLeft >= ticks){ // See if we need to insert in middle of queue
                    ticks -= total;
                    sleeping[i].ticksLeft -= ticks;
                    sleeping.insert({.thread = thread, .ticksLeft = ticks}, i);
                    return;
                }

                total += sleeping[i].ticksLeft;
            }

            sleeping.add_back({.thread = thread, .ticksLeft = ticks - sleeping.get_back().ticksLeft}); // Add to back
        }

        void Remove(thread_t* thread) final {
            for(auto it = sleeping.begin(); it != sleeping.end(); it++){
                SleepCounter& cnt = *it;
                if(cnt.thread == thread){
                    auto next = it;
                    next++;

                    next->ticksLeft += cnt.ticksLeft;

                    sleeping.remove(it);
                }
            }
        }
    };

    uint64_t GetSystemUptime(){
        return uptime;
    }

    uint32_t GetTicks(){
        return ticks;
    }

    uint32_t GetFrequency(){
        return frequency;
    }

    inline uint64_t MsToTicks(long ms){
        return ms * frequency / 1000;
    }

    timeval_t GetSystemUptimeStruct(){
        timeval_t tval;
        tval.seconds = uptime;
        tval.milliseconds = ticks * 1000 / frequency;
        return tval;
    }

    int TimeDifference(timeval_t newTime, timeval_t oldTime){
        long seconds = newTime.seconds - oldTime.seconds;
        int milliseconds = newTime.milliseconds - oldTime.milliseconds;

        return seconds * 1000 + milliseconds;
    }

    void SleepCurrentThread(timeval_t& time){
        long ticks = time.seconds * frequency + MsToTicks(time.milliseconds);
        SleepCurrentThread(ticks);
    }

    void Wait(long ms){
        assert(ms > 0);

        uint64_t ticksPerMs = (Timer::GetFrequency() / 1000);
        uint64_t timeMs = Timer::GetSystemUptime() * 1000 + (Timer::GetTicks() * ticksPerMs);
    
        while((Timer::GetSystemUptime() * 1000 + (Timer::GetTicks() * ticksPerMs)) - timeMs <= static_cast<unsigned long>(ms));
    }

    void SleepCurrentThread(long ticks){
        SleepBlocker blocker = SleepBlocker(ticks);
        Scheduler::BlockCurrentThread(blocker, sleepQueueLock);
    }

    // Timer handler
    void Handler(regs64_t *r) {
        ticks++;
        if(ticks >= frequency){
            uptime++;
            ticks -= frequency;
        }

        if(sleeping.get_length() && !(acquireTestLock(&sleepQueueLock))){
            if(sleeping.get_length()){ // Make sure the queue has not changed inbetween checking the length and acquiring the lock
                sleeping.get_front().ticksLeft--;

                while(sleeping.get_length() && sleeping.get_front().ticksLeft <= 0){
                    Scheduler::UnblockThread(sleeping.remove_at(0).thread);
                }
            }

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
        uint8_t l = (uint8_t)(divisor & 0xFF);
        uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );

        // Send the frequency divisor.
        outportb(0x40, l);
        outportb(0x40, h);
    }
}
