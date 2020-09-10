#pragma once

struct thread;

#include <list.h>

#include <spin.h>
#include <thread.h>
#include <logging.h>

class FilesystemLock {
    unsigned activeReaders = 0;
    lock_t fileLock = 0;

    FastList<thread*> readers;
    FastList<thread*> writers;
public:
    lock_t lock = 0;

    FilesystemLock() {}

    void AcquireRead(){
        acquireLock(&lock);

        activeReaders++;

        if(activeReaders == 1){
            acquireLock(&fileLock);
        }

        releaseLock(&lock);
    }

    void AcquireWrite(){
        acquireLock(&fileLock);
    }

    void ReleaseRead(){
        if(activeReaders == 1){
            releaseLock(&fileLock);
        }

        activeReaders--;
    }

    void ReleaseWrite(){
        releaseLock(&fileLock);
    }
};

class Semaphore : public Scheduler::GenericThreadBlocker{
protected:
    lock_t value = 0;
    lock_t blockedLock = 0;
public:
    Semaphore(int val){
        value = val;
    }

    void SetValue(int val){
        value = val;
    }

    void Wait();

    void WaitTimeout(long timeout);

    inline void Signal(){
        __sync_fetch_and_add(&value, 1);

        if(value >= 0){
            acquireLock(&blockedLock);
            if(blocked.get_length() > 0){
                Scheduler::UnblockThread(blocked.remove_at(0));
            }
            releaseLock(&blockedLock);
        }
    }
};