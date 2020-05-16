#pragma once

#define CHECK_DEADLOCK
#ifdef CHECK_DEADLOCK
#define acquireLock(lock) ({ \
    unsigned i = 0; \
    while(__sync_lock_test_and_set(lock, 1) && ++i < 0xFFFFFFFFF); \
    if( i >= 0xFFFFFFFFF) { asm volatile("int $0x0; movq ($0xf0f0f0f0deadbeef), %rax"); } \
    })
#else
#define acquireLock(lock) ({while(__sync_lock_test_and_set(lock, 1));})
#endif

#define releaseLock(lock) ({ __sync_lock_release(lock); });