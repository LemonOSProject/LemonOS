#pragma once

struct thread;

#include <List.h>

#include <Spinlock.h>
#include <Thread.h>
#include <Logging.h>

class Semaphore {
protected:
    lock_t value = 0;
    lock_t lock = 0;

    class SemaphoreBlocker : public ThreadBlocker{
        friend class Semaphore;
    public:
        SemaphoreBlocker* next = nullptr;
        SemaphoreBlocker* prev = nullptr;

        Semaphore* semaphore = nullptr;

        inline SemaphoreBlocker(Semaphore* sema) : semaphore(sema) {

        }

        void Interrupt(){
            interrupted = true;
            shouldBlock = false;

            acquireLock(&lock);
            if(semaphore){
                semaphore->blocked.remove(this);

                semaphore = nullptr;
            }
            releaseLock(&lock);
        }

        void Unblock(){
            shouldBlock = false;

            acquireLock(&lock);
            if(semaphore){
                semaphore->blocked.remove(this);
            }
            semaphore = nullptr;

            if(thread){
                thread->Unblock();
            }
            releaseLock(&lock);
        }

        ~SemaphoreBlocker(){
            if(semaphore){
                semaphore->blocked.remove(this);
            }
        }
    };

    FastList<SemaphoreBlocker*> blocked;
public:
    Semaphore(int val) : value(val) {};

    inline void SetValue(int val){
        value = val;
    }

    inline lock_t GetValue(){
        return value;
    }

    [[nodiscard]] bool Wait();
    [[nodiscard]] bool WaitTimeout(long& timeout);

    void Signal();
};

class ReadWriteLock {
    unsigned activeReaders = 0;
    lock_t fileLock = 0;
    lock_t lock = 0;

    bool writerAcquiredLock = false; // Whether or not the writer has acquired lock (but not fileLock) 

    FastList<thread*> readers;
    FastList<thread*> writers;
public:

    ReadWriteLock() {}

    inline void AcquireRead(){
        acquireLock(&lock);

        activeReaders++;

        if(activeReaders == 1){
            acquireLock(&fileLock);
        }

        releaseLock(&lock);
    }

    inline void AcquireWrite(){
        acquireLock(&lock); // Stop more threads from reading
        acquireLock(&fileLock);
    }

    inline bool TryAcquireWrite(){
        if(!writerAcquiredLock && acquireTestLock(&lock)){ // Stop more threads from reading
            return true;
        }
        writerAcquiredLock = true; // No need to acquire lock next time when we come back

        if(acquireTestLock(&fileLock)){
            return true;
        }
        writerAcquiredLock = false;

        return false;
    }

    inline void ReleaseRead(){
        if(activeReaders == 1){
            releaseLock(&fileLock);
        }

        activeReaders--;
    }

    inline void ReleaseWrite(){
        releaseLock(&fileLock);
        releaseLock(&lock);
    }
};

using FilesystemLock = ReadWriteLock;