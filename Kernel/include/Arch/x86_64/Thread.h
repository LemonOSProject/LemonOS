#pragma once

#include <CPU.h>
#include <List.h>
#include <MiscHdr.h>
#include <Signal.h>
#include <Spinlock.h>
#include <Timer.h>

#include <abi-bits/pid_t.h>

#define THREAD_TIMESLICE_DEFAULT 10

enum {
    ThreadStateRunning, // Thread is running
    ThreadStateBlocked, // Thread is blocked, do not schedule
    ThreadStateZombie,  // Waiting on thread to close a resource/exit a syscall
    ThreadStateDying,   // Thread is actively being killed
};

class Process;
struct Thread;

class ThreadBlocker {
    friend struct Thread;

protected:
    lock_t lock = 0;
    Thread* thread = nullptr;

    bool shouldBlock = true;  // If Unblock() is called before the thread is blocked or the lock is acquired then tell
                              // the thread not to block
    bool interrupted = false; // Returned by Block so the thread knows it has been interrupted
    bool removed = false;     // Has the blocker been removed from queue(s)?
public:
    virtual ~ThreadBlocker() = default;

    virtual void Interrupt(); // A blocker may get interrupted because a thread is getting killed.
    virtual void Unblock();

    inline bool ShouldBlock() { return shouldBlock; }
    inline bool WasInterrupted() { return interrupted; }
};

class GenericThreadBlocker : public ThreadBlocker {
public:
    inline void Interrupt() {}
};

using FutexThreadBlocker = GenericThreadBlocker;

struct Thread {
    lock_t lock = 0;      // Thread lock
    lock_t stateLock = 0; // Thread state lock

    Process* parent; // Parent Process
    pid_t tid = 1;

    void* stack = nullptr;       // Pointer to the initial stack
    void* stackLimit = nullptr;  // The limit of the stack
    void* kernelStack = nullptr; // Kernel Stack
    uint32_t timeSlice = THREAD_TIMESLICE_DEFAULT;
    uint32_t timeSliceDefault = THREAD_TIMESLICE_DEFAULT;
    RegisterContext registers;   // Registers
    RegisterContext lastSyscall; // Last system call
    void* fxState;               // State of the extended registers

    Thread* next = nullptr; // Next thread in queue
    Thread* prev = nullptr; // Previous thread in queue

    uint8_t priority = 0;               // Thread priority
    uint8_t state = ThreadStateRunning; // Thread state

    uint64_t fsBase = 0;

    bool blockTimedOut = false;
    ThreadBlocker* blocker = nullptr;

    uint64_t pendingSignals = 0; // Bitmap of pending signals
    uint64_t signalMask = 0;     // Masked signals

    Thread(class Process* _parent, pid_t _tid);

    /////////////////////////////
    /// \brief Dispatch a signal to the thread
    /////////////////////////////
    void Signal(int signal);

    /////////////////////////////
    /// \brief Call the signal handler for the first pending signal
    /////////////////////////////
    void HandlePendingSignal(RegisterContext* regs);

    /////////////////////////////
    /// \brief Block a thread
    ///
    ///	Blocks the thread. It is essential that an interrupted block is handled correctly
    ///
    /// \param blocker ThreadBlocker
    ///
    /// \return Whether or not the thread was interrupted
    /////////////////////////////
    [[nodiscard]] bool Block(ThreadBlocker* blocker);

    /////////////////////////////
    /// \brief Block a thread with timeout
    ///
    /// Blocks the thread with a timeout period. It is essential that an interrupted block is handled correctly.
    ///
    /// \param blocker ThreadBlocker
    /// \param timeout Refernce to timeout period in microseconds. Will be filled with a value <= 0 when a timeout
    /// occurs.
    ///
    /// \return Whether or not the thread was interrupted
    /////////////////////////////
    [[nodiscard]] bool Block(ThreadBlocker* blocker, long& usTimeout);

    /////////////////////////////
    /// \brief Put the thread to sleep (blocking)
    /////////////////////////////
    void Sleep(long us);

    /////////////////////////////
    /// \brief Write data to filesystem node
    ///
    /// Write data to filesystem node
    ///
    /// \param off Offset where data should be written
    /// \param size Amount of data (in bytes) to write
    ///
    /// \return Bytes written or if negative an error code
    /////////////////////////////
    void Unblock();
};