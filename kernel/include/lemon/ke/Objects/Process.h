#pragma once

#include <abi/types.h>

#include <CPU.h>
#include <Compiler.h>
#include <ELF.h>
#include <Errno.h>
#include <Error.h>
#include <Fs/Filesystem.h>
#include <Hash.h>
#include <List.h>
#include <MM/AddressSpace.h>
#include <Objects/Handle.h>
#include <Objects/KObject.h>
#include <Objects/Watcher.h>
#include <RefPtr.h>
#include <Thread.h>
#include <TimerEvent.h>
#include <Vector.h>

#define PROC_USER_SHARED_DATA_BASE 0x7000C00000
#define PROC_PEB_BASE 0x7000800000

class Process : public KernelObject {
    DECLARE_KOBJECT(Process);

    friend class Thread;
    friend void KernelProcess();

public:
    enum {
        Process_Running = 0,
        Process_Dying = 1,
        Process_Dead = 3,
    };

    static FancyRefPtr<Process> create_idle_process(const char* name);
    static FancyRefPtr<Process> create_kernel_process(void* entry, const char* name, Process* parent);
    static ErrorOr<FancyRefPtr<Process>> create_elf_process(const FancyRefPtr<File>& elf, const Vector<String>& argv,
                                                          const Vector<String>& envp, const char* execPath,
                                                          Process* parent);
    ALWAYS_INLINE static Process* current() {
        if (Thread::current())
            return Thread::current()->parent;

        return nullptr;
    }

    ~Process();

    /////////////////////////////
    /// \brief Destroy Process KernelObject
    ///
    /// Will send SIGKILL to process if it is not already dead/dying.
    /// Also removes Process object from parent, such as happens when calling waitpid()
    /////////////////////////////
    void Destroy();

    /////////////////////////////
    /// \brief Kills the process
    ///
    /// Responsible for acutally killing the process.
    /// Will free up most resources and pass the process to the reaper thread.
    ///
    /// Does not remove process object from memory.
    /// Will stay in memory until both the reaper thread has finished with it,
    /// and the parent has called Destroy() or waitpid().
    /////////////////////////////
    void die();

    void start();

    /////////////////////////////
    /// \brief Watch process with watcher
    ///
    /// Signals watcher when the process dies.
    ///
    /// \param watcher Watcher object
    /// \param events Ignored (for now)
    /////////////////////////////
    void Watch(KernelObjectWatcher* watcher, KOEvent events) override;
    void Unwatch(KernelObjectWatcher* watcher) override;

    /////////////////////////////
    /// \brief Fork Process
    ///
    /// Clones this process and forks the AddressSpace
    /// with copy-on-write (COW)
    ///
    /// \return Pointer to new process
    /////////////////////////////
    FancyRefPtr<Process> fork();

    /////////////////////////////
    /// \brief Load ELF into the process' address space
    ///
    /// Loads ELF data into the process' address space,
    /// and sets up the stack.
    /// e.g. Used in create_elf_process or execve syscall.
    ///
    /// \return Entry point of the ELF (either dynamic linker or executable itself)
    /////////////////////////////
    ErrorOr<uintptr_t> load_elf(uintptr_t* stackPointer, ELFData& elfInfo, const Vector<String>& argv,
                               const Vector<String>& envp, const char* execPath);

    /////////////////////////////
    /// \brief Replaces the calling process with a new executable
    ///
    /// Assumes it is running within a syscall.
    ///
    /////////////////////////////
    Error execve(ELFData& exe, const Vector<String>& argv, const Vector<String>& envp, const char* execPath);

    /////////////////////////////
    /// \brief Kills all other threads
    ///
    /// Assumes the function is called from a thread of the process
    /////////////////////////////
    void kill_all_other_threads();

    /////////////////////////////
    /// \brief Retrieve Process PID
    /////////////////////////////
    ALWAYS_INLINE pid_t pid() const {
        return m_pid;
    }
    /////////////////////////////
    /// \brief Retrieve Process State
    /////////////////////////////
    ALWAYS_INLINE int state() const {
        return m_state;
    }
    /////////////////////////////
    /// \brief Retrieve Whether Process is Dead
    /////////////////////////////
    ALWAYS_INLINE int is_dead() const {
        return m_state == Process_Dead;
    }
    /////////////////////////////
    /// \brief Retrieve Whether Process is the Idle Process of a CPU
    /////////////////////////////
    ALWAYS_INLINE int is_cpu_idle_process() const {
        return m_isIdleProcess;
    }

    /////////////////////////////
    /// \brief Retrieve Main Thread
    ///
    /// The main thread is the original thread and should
    /// always have a TID of 1.
    /// Lemon OS does not currently support killing the main thread.
    /////////////////////////////
    ALWAYS_INLINE FancyRefPtr<Thread> get_main_thread() {
        return m_mainThread;
    }
    /////////////////////////////
    /// \brief Retrieve thread using TID
    ///
    /// \param tid Thread ID of the thread to retrieve
    ///
    /// The main thread is the original thread and should
    /// always have a TID of 1.
    ///
    /// \return Return pointer to thread on success, FancyRefPtr<nullptr> on false
    /////////////////////////////
    ALWAYS_INLINE FancyRefPtr<Thread> get_thread_from_tid(pid_t tid) {
        ScopedSpinLock acq(m_processLock);
        return GetThreadFromTID_Unlocked(tid);
    }

    FancyRefPtr<Thread> create_child_thread(void* entry, void* stack);
    const List<FancyRefPtr<Thread>>& Threads() {
        return m_threads;
    }

    ALWAYS_INLINE PageMap* get_page_map() {
        return addressSpace->get_page_map();
    }

    /////////////////////////////
    /// \brief Get size of handle vector
    ///
    /// Includes invalid/closed handles
    /////////////////////////////
    ALWAYS_INLINE unsigned handle_count() const {
        return m_handles.size();
    }

    /////////////////////////////
    /// \brief Allocate Handle
    ///
    /// \param fd Reference pointer to valid KernelObject
    /// \return ID of new file descriptor
    /////////////////////////////
    ALWAYS_INLINE le_handle_t allocate_handle(FancyRefPtr<KernelObject> ko, bool closeOnExec = false) {
        ScopedSpinLock lockHandles(m_handleLock);

        Handle h;
        h.ko = std::move(ko);
        h.closeOnExec = closeOnExec;

        int i = 0;
        for (; i < static_cast<int>(m_handles.get_length()); i++) {
            if (!m_handles[i].IsValid()) {
                h.id = i;
                m_handles[i] = std::move(h);
                return i;
            }
        }

        h.id = i;
        m_handles.add_back(h);

        return i;
    }

    template <KernelObjectDerived T> ALWAYS_INLINE le_handle_t allocate_handle(FancyRefPtr<T> ko, bool cloExec = false) {
        return allocate_handle(static_pointer_cast<KernelObject>(std::move(ko)), cloExec);
    }

    /////////////////////////////
    /// \brief Retrieves Handle object corresponding to id
    ///
    /// \return Handle object on success
    /// \return null handle when \a id is invalid
    /////////////////////////////
    ALWAYS_INLINE Handle get_handle(le_handle_t id) {
        ScopedSpinLock lockHandles(m_handleLock);
        if (id < 0 || id >= m_handles.size()) {
            return HANDLE_NULL; // No such handle
        }

        return m_handles[id];
}

    template <typename T> ALWAYS_INLINE ErrorOr<FancyRefPtr<T>> get_handle_as(le_handle_t id) {
        Handle h = get_handle(id);
        if (!h.IsValid()) {
            return Error{EBADF};
        }

        if (h.ko->is_type(T::type_id())) {
            return static_pointer_cast<T>(h.ko);
        }

        return Error{EINVAL};
    }

    /////////////////////////////
    /// \brief Replace handle
    /////////////////////////////
    ALWAYS_INLINE Error handle_replace(le_handle_t id, Handle newHandle) {
        if (id < 0) {
            return EBADF;
        }

        ScopedSpinLock lockHandles(m_handleLock);
        if(id >= m_handles.size()) {
            // TODO: Limit of the number of open handles
            m_handles.resize(id + 1);
        }

        newHandle.id = id;
        m_handles.at(id) = std::move(newHandle);

        return ERROR_NONE;
    }

    ALWAYS_INLINE Error handle_set_cloexec(le_handle_t id, bool value) {
        if (id < 0) {
            return EBADF;
        }

        ScopedSpinLock lockHandles(m_handleLock);
        if (id >= m_handles.size()) {
            return EBADF;
        }

        m_handles.at(id).closeOnExec = value;
        return ERROR_NONE;
    }

    /////////////////////////////
    /// \brief Destroy handle
    ///
    /// Note other threads may still hold a reference to the KernelObject
    ///
    /// \param id Handle to destroy
    /// \return 0 on success, EBADF when fd is out of range
    /////////////////////////////
    ALWAYS_INLINE int handle_destroy(le_handle_t id) {
        ScopedSpinLock lockHandles(m_handleLock);
        if (id >= m_handles.size()) {
            return EBADF; // No such handle
        }

        // Deferences the KernelObject
        m_handles[id] = {};
        return 0;
    }

    ALWAYS_INLINE Handle stdin() {
        return m_handles[0];
    };
    ALWAYS_INLINE Handle stdout() {
        return m_handles[1];
    };
    ALWAYS_INLINE Handle stderr() {
        return m_handles[2];
    };

    ALWAYS_INLINE void register_child(const FancyRefPtr<Process>& child) {
        ScopedSpinLock lock(m_processLock);
        m_children.add_back(child);
    }

    ALWAYS_INLINE FancyRefPtr<Process> find_child_by_pid(pid_t pid) {
        ScopedSpinLock lock(m_processLock);
        for (auto& child : m_children) {
            if (child->pid() == pid) {
                return child;
            }
        }

        return nullptr;
    }

    /////////////////////////////
    /// \brief Find first dead child and remove
    ///
    /// \return Reference pointer to dead child
    /// \return nullptr when no dead children
    /////////////////////////////
    ALWAYS_INLINE FancyRefPtr<Process> remove_dead_child() {
        ScopedSpinLock lock(m_processLock);
        FancyRefPtr<Process> proc;

        for (auto it = m_children.begin(); it != m_children.end(); it++) {
            if ((*it)->state() == Process_Dead) {
                proc = std::move(*it);
                m_children.remove(it);
                break;
            }
        }

        if (proc.get()) {
            ScopedSpinLock lockChild(proc->m_processLock);
            proc->m_parent = nullptr;
            return proc;
        }

        return nullptr;
    }
    /////////////////////////////
    /// \brief Find dead child with PID \a pid and remove
    ///
    /// \param pid PID of child to remove
    /////////////////////////////
    ALWAYS_INLINE FancyRefPtr<Process> remove_dead_child(pid_t pid) {
        ScopedSpinLock lock(m_processLock);
        for (auto it = m_children.begin(); it != m_children.end(); it++) {
            if ((*it)->pid() == pid) {
                assert((*it)->is_dead());

                acquireLock(&(*it)->m_processLock);
                FancyRefPtr<Process> proc = std::move(*it);
                proc->m_parent = nullptr;
                m_children.remove(it);
                releaseLock(&proc->m_processLock);
                return proc;
            }
        }
        assert(!"Could not find process!");
    }

    /////////////////////////////
    /// \brief Wait for any child to die and remove
    ///
    /// \return 0 on success, otherwise error code
    /////////////////////////////
    ALWAYS_INLINE int wait_for_child_to_die(FancyRefPtr<Process>& ptr) {
        auto child = remove_dead_child();
        if (child.get()) {
            ptr = std::move(child);
            return 0;
        }

    retry:
        acquireLock(&m_processLock);
        if (!m_children.get_length()) {
            releaseLock(&m_processLock);
            return -ECHILD; // No children to wait for
        }

        KernelObjectWatcher watcher;
        for (auto& child : m_children) {
            watcher.Watch(child, KOEvent::ProcessTerminated);
        }
        releaseLock(&m_processLock);

        bool wasInterrupted = watcher.wait();

        child = remove_dead_child();
        if (child.get()) {
            ptr = std::move(child);
            return 0;
        } else if (wasInterrupted) {
            return -EINTR; // We were interrupted
        }

        goto retry; // Keep waiting
        return 0;
    }

    ALWAYS_INLINE void set_alarm(unsigned int seconds) {
        ScopedSpinLock lockProcess(m_processLock);

        if (seconds == 0) {
            m_alarmEvent = nullptr;
            return;
        }

        m_alarmEvent = new Timer::TimerEvent(
            seconds * 1000000,
            [](void* thread) {
                Thread* t = reinterpret_cast<Thread*>(thread);

                t->signal(SIGALRM);
            },
            m_mainThread.get());
    }

    char name[NAME_MAX + 1];

    FancyRefPtr<File> workingDir;
    char workingDirPath[PATH_MAX + 1];

    // POSIX permissions
    int32_t euid = 0; // Effective UID
    int32_t uid = 0;
    int32_t egid = 0; // Effective GID
    int32_t gid = 0;

    // POSIX signal handlers
    SignalHandler signalHandlers[SIGNAL_MAX];

    timeval creationTime;     // When the process was created
    uint64_t activeTicks = 0; // How many ticks this process has been active

    AddressSpace* addressSpace = nullptr;

    MappedRegion* pebRegion = nullptr;
    MappedRegion* userSharedDataRegion = nullptr;

    HashMap<uintptr_t, List<FutexThreadBlocker*>*> futexWaitQueue = HashMap<uintptr_t, List<FutexThreadBlocker*>*>(8);

    int exitCode = 0;

    // Handle table
    Vector<Handle> m_handles;

    lock_t futexLock = 0;          // Should be acquired when modifying futex block queue

private:
    Process(pid_t pid, const char* name, const char* workingDir, Process* parent);

    FancyRefPtr<Thread> GetThreadFromTID_Unlocked(pid_t tid);
    void map_user_shared_data();
    void map_process_environment_block();

    // Assumes calling thread is running in the address space of the process
    void initialize_peb();

    lock_t m_processLock = 0;        // Should be acquired when modifying the data structure
    lock_t m_watchingLock = 0;       // Should be acquired when modifying watching processes
    lock_t m_fileDescriptorLock = 0; // Should be acquired when modifying file descriptors
    lock_t m_handleLock = 0;         // Should be acquired when modifying handles
    pid_t m_pid;                     // Process ID (PID)

    bool m_started = false; // Has the process been started?

    MappedRegion* m_signalTrampoline = nullptr;

    int m_state = Process_Running;
    bool m_isIdleProcess = false;

    // Give thread pointers to other processes as reference counted.
    // If the process ends whilst another process/structure
    // holds the thread that could be catastrophic
    int64_t m_nextThreadID = 1;

    FancyRefPtr<Thread> m_mainThread;
    List<FancyRefPtr<Thread>> m_threads;
    List<FancyRefPtr<Process>> m_children; // Same goes for child processes

    // Processes generally have two references to each child
    // 1. m_children - This is used to enumerate children,
    // dead processes are also stored here until waitpid() is called,
    // or Destroy() is called
    // 2. m_handles - Processes have a handle to their child.
    // When Destroy() is called on the child both references are removed
    //
    // The process may choose not to take the process handle.
    // This facilitates both Lemon-specific and standard UNIX handling
    // of processes

    // Watcher objects watching the process
    List<KernelObjectWatcher*> m_watching;

    // Used for SIGALARM
    FancyRefPtr<Timer::TimerEvent> m_alarmEvent = nullptr;

    Process* m_parent = nullptr;
};
