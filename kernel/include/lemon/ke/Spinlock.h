#pragma once

typedef volatile int lock_t;

#include <CPU.h>
#include <Compiler.h>

//#define CHECK_DEADLOCK
#ifdef CHECK_DEADLOCK
#include <Assert.h>

#define acquireLock(lock)                                                                                              \
    ({                                                                                                                 \
        unsigned i = 0;                                                                                                \
        while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE) && ++i < 0xFFFFFFF)                                     \
            asm("pause");                                                                                              \
        assert(i < 0xFFFFFFF);                                                                                        \
    })

#define acquireLockIntDisable(lock)                                                                                    \
    ({                                                                                                                 \
        unsigned i = 0;                                                                                                \
        assert(CheckInterrupts());                                                                                     \
        asm volatile("cli");                                                                                           \
        while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE) && ++i < 0xFFFFFFF)                                     \
            asm volatile("sti; pause; cli");                                                                           \
        assert(i < 0xFFFFFFF);                                                                                        \
    })

#else
#define acquireLock(lock)                                                                                              \
    ({                                                                                                                 \
        while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE))                                                         \
            asm("pause");                                                                                              \
    })

#define acquireLockIntDisable(lock)                                                                                    \
    ({                                                                                                                 \
        asm volatile("cli");                                                                                           \
        while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE))                                                         \
            asm volatile("sti; pause; cli");                                                                           \
    })

#endif

#define releaseLock(lock) ({ __atomic_store_n(lock, 0, __ATOMIC_RELEASE); });

#define acquireTestLock(lock)                                                                                          \
    ({                                                                                                                 \
        int status;                                                                                                    \
        status = __atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE);                                                       \
        status;                                                                                                        \
    })

template <bool disableInterrupts = false> class ScopedSpinLock final {
public:
    ALWAYS_INLINE ScopedSpinLock(lock_t& lock) : m_lock(lock) {
        if constexpr (disableInterrupts) {
            m_irq = CheckInterrupts();
            if (m_irq) {
                acquireLockIntDisable(&m_lock);
            } else {
                acquireLock(&m_lock);
            }
        } else {
            acquireLock(&m_lock);
        }
    }
    ALWAYS_INLINE ~ScopedSpinLock() {
        releaseLock(&m_lock);

        if constexpr (disableInterrupts) {
            if (m_irq) {
                asm volatile("sti");
            }
        }
    }

private:
    lock_t& m_lock;
    bool m_irq;
};