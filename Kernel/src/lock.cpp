#include <Lock.h>

#include <Scheduler.h>
#include <Timer.h>
#include <CPU.h>
#include <Logging.h>

bool Semaphore::Wait(){
    acquireLock(&lock);
    __sync_fetch_and_sub(&value, 1);

    if(value < 0){
        SemaphoreBlocker blocker(this);
        blocked.add_back(&blocker);

        releaseLock(&lock);

        return Scheduler::GetCurrentThread()->Block(&blocker);
    }
    releaseLock(&lock);

    return false;
}

bool Semaphore::WaitTimeout(long& timeout){
    acquireLock(&lock);
    __sync_fetch_and_sub(&value, 1);

    if(value < 0){
        SemaphoreBlocker blocker(this);
        blocked.add_back(&blocker);

        releaseLock(&lock);

        return Scheduler::GetCurrentThread()->Block(&blocker, timeout);
    }
    releaseLock(&lock);

    return false;
}

void Semaphore::Signal(){
    acquireLock(&lock);

    __sync_fetch_and_add(&value, 1);
    if(blocked.get_length() > 0){
        blocked.get_front()->Unblock();
    }

    releaseLock(&lock);
}