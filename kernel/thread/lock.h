#pragma once

#include <assert.h>
#include <cpu.h>

namespace thread {

template<typename T>
concept Lockable = requires (T t) {
    { t.lock() };
    { t.unlock() };
};

class TicketLock {
public:
    void lock() {
        int our_ticket = __atomic_fetch_add(&m_next, 1, __ATOMIC_ACQ_REL);
        
        // Safe to disable interrupts here,
        // as if another thread on the same CPU has the lock,
        // it cannot be interrupted as it has also disabled IRQs
        m_disable_ints.disable();
        while (m_current != our_ticket)
            asm volatile("pause");
    }

    void unlock() {
        __atomic_fetch_add(&m_current, 1, __ATOMIC_ACQ_REL);
        m_disable_ints.enable();
    }

private:
    hal::cpu::InterruptDisabler m_disable_ints {false};

    volatile int m_next = 0;
    volatile int m_current = 0;
};

template<Lockable T>
class LockGuard {
public:
    LockGuard(T &lock) : m_lock(lock) {
        m_lock.lock();
        m_is_locked = true;
    }

    ~LockGuard() {
        if (m_is_locked)
            m_lock.unlock();
        m_is_locked = false;
    }

    void lock() {
        assert(!m_is_locked);

        m_lock.lock();
        m_is_locked = true;
    }

    void unlock() {
        assert(m_is_locked);

        m_lock.unlock();
        m_is_locked = false;
    }

private:
    T &m_lock;
    bool m_is_locked;
};

}
