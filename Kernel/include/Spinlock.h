#pragma once

typedef volatile int lock_t;

#include <CPU.h>
#include <Compiler.h>

#define CHECK_DEADLOCK
#ifdef CHECK_DEADLOCK
#include <Assert.h>

#define acquireLock(lock)                                                                                              \
    ({                                                                                                                 \
        unsigned i = 0;                                                                                                \
        while (__sync_lock_test_and_set(lock, 1) && ++i < 0xFFFFFFF)                                                   \
            asm("pause");                                                                                              \
        if (i >= 0x2FFFFFFF) {                                                                                          \
            assert(!"Deadlock!");                                                                                      \
        }                                                                                                              \
    })

#define acquireLockIntDisable(lock)                                                                                    \
    ({                                                                                                                 \
        unsigned i = 0;                                                                                                \
        assert(CheckInterrupts());                                                                                     \
        asm volatile("cli");                                                                                           \
        while (__sync_lock_test_and_set(lock, 1) && ++i < 0xFFFFFFF)                                                   \
            asm("sti; pause; cli");                                                                                    \
        if (i >= 0x2FFFFFFF) {                                                                                          \
            assert(!"Deadlock!");                                                                                      \
        }                                                                                                              \
    })

#else
#define acquireLock(lock)                                                                                              \
    ({                                                                                                                 \
        while (__sync_lock_test_and_set(lock, 1))                                                                      \
            asm("pause");                                                                                              \
    })
#endif

#define releaseLock(lock) ({ __sync_lock_release(lock); });

#define acquireTestLock(lock)                                                                                          \
    ({                                                                                                                 \
        int status;                                                                                                    \
        status = __sync_lock_test_and_set(lock, 1);                                                                    \
        status;                                                                                                        \
    })

template <bool disableInterrupts = false> class ScopedSpinLock final {
public:
    ALWAYS_INLINE ScopedSpinLock(lock_t& lock) : m_lock(lock) {
        if constexpr (disableInterrupts) {
            assert(CheckInterrupts()); // Ensure interrupts are currently enabled
            acquireLockIntDisable(&m_lock);
        } else {
            acquireLock(&m_lock);
        }
    }
    ALWAYS_INLINE ~ScopedSpinLock() {
        releaseLock(&m_lock);

        if constexpr (disableInterrupts) {
            asm volatile("sti");
        }
    }

private:
    lock_t& m_lock;
};