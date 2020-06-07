#pragma once

struct thread;

#include <list.h>

typedef struct {
    volatile int value = 1;
    FastList<thread*> waiting;
} semaphore_t;
typedef volatile int lock_t;

#include <spin.h>

static inline void SemaphoreWait(semaphore_t* s){
    __sync_fetch_and_sub(&s->value, 1);
    while(s->value < 0) asm("pause");
}

static inline void SemaphoreSignal(semaphore_t* s){
    __sync_fetch_and_add(&s->value, 1);
}