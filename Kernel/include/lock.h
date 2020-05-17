#pragma once

struct thread;
template <typename T>
class FastList;

typedef struct {
    volatile int value = 1;
    FastList<thread*>* waiting;
} semaphore_t;
typedef volatile int lock_t;

#define CHECK_DEADLOCK
#ifdef CHECK_DEADLOCK

#define acquireLock(lock) ({ \
    volatile unsigned i = 0; \
    while(__sync_lock_test_and_set(lock, 1) && ++i < 0xFFFFFFF) asm("pause"); \
    if( i >= 0xFFFFFFF) { asm volatile("int $0xE;"); } \
    })
#else
#define acquireLock(lock) ({while(__sync_lock_test_and_set(lock, 1)) asm("pause");})
#endif

#define releaseLock(lock) ({ __sync_lock_release(lock); });

#define acquireTestLock(lock) ({int status; status = __sync_lock_test_and_set(lock, 1); status;})


static inline void SemaphoreWait(semaphore_t* s){
    __sync_fetch_and_sub(&s->value, 1);
    while(s->value < 0) asm("pause");
}

static inline void SemaphoreSignal(semaphore_t* s){
    __sync_fetch_and_add(&s->value, 1);
}