#pragma once

struct thread;

#include <list.h>

#include <spin.h>
#include <thread.h>
#include <logging.h>

class Semaphore {
protected:
    lock_t value = 0;
    lock_t lock = 0;

    class SemaphoreBlocker : public ThreadBlocker{
        friend class Semaphore;
    public:
        SemaphoreBlocker* next = nullptr;
        SemaphoreBlocker* prev = nullptr;

        Semaphore* semaphore;

        inline SemaphoreBlocker(Semaphore* sema) : semaphore(sema) {

        }

        void Interrupt(){
            interrupted = true;
            shouldBlock = false;

            acquireLock(&semaphore->lock);
            semaphore->blocked.remove(this);
            releaseLock(&semaphore->lock);
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

    FastList<thread*> readers;
    FastList<thread*> writers;
public:
    lock_t lock = 0;

    ReadWriteLock() {}

    void AcquireRead(){
        acquireLock(&lock);

        activeReaders++;

        if(activeReaders == 1){
            acquireLock(&fileLock);
        }

        releaseLock(&lock);
    }

    void AcquireWrite(){
        acquireLock(&lock); // Stop more threads from reading
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
        releaseLock(&lock);
    }
};

using FilesystemLock = ReadWriteLock;