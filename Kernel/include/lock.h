#pragma once

struct thread;

#include <list.h>

typedef struct {
    volatile int value = 1;
    FastList<thread*> waiting;
} semaphore_t;

typedef volatile int lock_t;

#include <spin.h>

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

static inline void SemaphoreBlock(semaphore_t* s){

}

static inline void SemaphoreWait(semaphore_t* s){
    __sync_fetch_and_sub(&s->value, 1);
    while(s->value < 0) asm("pause");
}

static inline void SemaphoreWait(semaphore_t& s){
    SemaphoreWait(&s);
}

static inline void SemaphoreSignal(semaphore_t* s){
    __sync_fetch_and_add(&s->value, 1);
}

static inline void SemaphoreSignal(semaphore_t& s){
    SemaphoreSignal(&s);
}