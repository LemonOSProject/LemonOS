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
    List<SleepCounter> sleeping; // Sleeping threads

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

    int TimeDifference(timeval_t& newTime, timeval_t& oldTime){
        long seconds = newTime.seconds - oldTime.seconds;
        int milliseconds = newTime.milliseconds - oldTime.milliseconds;

        return seconds * 1000 + milliseconds;
    }

    void SleepCurrentThread(timeval_t& time){
        long ticks = time.seconds * frequency + MsToTicks(time.milliseconds);
        SleepCurrentThread(ticks);
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
            sleeping.get_front().ticksLeft--;

            while(sleeping.get_length() && sleeping.get_front().ticksLeft <= 0){
                Scheduler::UnblockThread(sleeping.remove_at(0).thread);
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
