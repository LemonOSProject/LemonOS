#pragma once

/*extern "C"{
    void acquireLock(volatile void* lock);
    void releaseLock(volatile void* lock);
}*/

#define CHECK_DEADLOCK
#ifdef CHECK_DEADLOCK
#define acquireLock(lock) ({ \
    unsigned i = 0; \
    while(__sync_lock_test_and_set(lock, 1) && ++i < 0xFFFFFFF); \
    if( i >= 0xFFFFFFF) asm volatile("int $0x0"); \
    })
#else
#define acquireLock(lock) ({while(__sync_lock_test_and_set(lock, 1));})
#endif

#define releaseLock(lock) ({ __sync_lock_release(lock); });