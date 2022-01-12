#include <Scheduler.h>

#include <ABI.h>
#include <APIC.h>
#include <CPU.h>
#include <Debug.h>
#include <ELF.h>
#include <Fs/Initrd.h>
#include <IDT.h>
#include <List.h>
#include <Lock.h>
#include <Logging.h>
#include <MM/KMalloc.h>
#include <Paging.h>
#include <Panic.h>
#include <PhysicalAllocator.h>
#include <SMP.h>
#include <Serial.h>
#include <String.h>
#include <TSS.h>
#include <Timer.h>

extern "C" [[noreturn]] void TaskSwitch(RegisterContext* r, uint64_t pml4);

extern "C" void IdleProcess();

void KernelProcess();

namespace Scheduler {
int schedulerLock = 0;
bool schedulerReady = false;

lock_t processesLock = 0;
List<FancyRefPtr<Process>>* processes;

lock_t destroyedProcessesLock = 0;
List<FancyRefPtr<Process>>* destroyedProcesses;

unsigned processTableSize = 512;
pid_t nextPID = 1;

void Schedule(void*, RegisterContext* r);

inline void InsertThreadIntoQueue(Thread* thread) { GetCPULocal()->runQueue->add_back(thread); }

inline void RemoveThreadFromQueue(Thread* thread) { GetCPULocal()->runQueue->remove(thread); }

void InsertNewThreadIntoQueue(Thread* thread) {
    CPU* cpu = SMP::cpus[0];
    for (unsigned i = 1; i < SMP::processorCount; i++) {
        if (SMP::cpus[i]->runQueue->get_length() < cpu->runQueue->get_length()) {
            cpu = SMP::cpus[i];
        }

        if (!cpu->runQueue->get_length()) {
            break;
        }
    }

    asm("sti");
    acquireLock(&cpu->runQueueLock);
    asm("cli");
    cpu->runQueue->add_back(thread);
    releaseLock(&cpu->runQueueLock);
    asm("sti");
}

void Initialize() {
    processes = new List<FancyRefPtr<Process>>();
    destroyedProcesses = new List<FancyRefPtr<Process>>();

    CPU* cpu = GetCPULocal();

    for (unsigned i = 0; i < SMP::processorCount; i++) {
        FancyRefPtr<Process> idleProcess = Process::CreateIdleProcess((String("idle_cpu") + to_string(i)).c_str());
        SMP::cpus[i]->idleProcess = idleProcess.get();
        SMP::cpus[i]->idleThread = idleProcess->GetMainThread().get();
    }

    for (unsigned i = 0; i < SMP::processorCount; i++) {
        SMP::cpus[i]->runQueue->clear();
        releaseLock(&SMP::cpus[i]->runQueueLock);
    }

    IDT::RegisterInterruptHandler(IPI_SCHEDULE, Schedule);

    auto kproc = Process::CreateKernelProcess((void*)KernelProcess, "Kernel", nullptr);
    kproc->Start();

    cpu->currentThread = nullptr;
    schedulerReady = true;
    asm("sti; int $0xfd;"); // IPI_SCHEDULE
    assert(!"Failed to initiailze scheduler!");
}

void RegisterProcess(FancyRefPtr<Process> proc) {
    ScopedSpinLock acq(processesLock);
    processes->add_back(std::move(proc));
}

void MarkProcessForDestruction(Process* proc) {
    ScopedSpinLock lockProcesses(processesLock);
    ScopedSpinLock lockDestroyedProcesses(destroyedProcessesLock);

    for (auto it = processes->begin(); it != processes->end(); it++) {
        if (it->get() == proc) {
            destroyedProcesses->add_back(*it);
            processes->remove(it);
            return;
        }
    }

    assert(!"Failed to mark process for destruction!");
}

pid_t GetNextPID() { return nextPID++; }

FancyRefPtr<Process> FindProcessByPID(pid_t pid) {
    ScopedSpinLock lockProcesses(processesLock);
    for (auto& proc : *processes) {
        if (proc->PID() == pid)
            return proc;
    }

    return nullptr;
}

pid_t GetNextProcessPID(pid_t pid) {
    ScopedSpinLock lockProcesses(processesLock);
    for (auto it = processes->begin(); it != processes->end(); it++) {
        if (it->get()->PID() > pid) { // Found process with PID greater than pid
            return it->get()->PID();
        }
    }

    return 0; // Could not find process, return as if end of list
}

void Yield() {
    CPU* cpu = GetCPULocal();

    if (cpu->currentThread) {
        cpu->currentThread->timeSlice = 0;
    }
    asm("int $0xFD"); // Send schedule IPI to self
}

void Tick(RegisterContext* r) {
    if (!schedulerReady)
        return;

    APIC::Local::SendIPI(0, ICR_DSH_OTHER, ICR_MESSAGE_TYPE_FIXED, IPI_SCHEDULE);

    Schedule(nullptr, r);
}

void Schedule(__attribute__((unused)) void* data, RegisterContext* r) {
    CPU* cpu = GetCPULocal();

    if (cpu->currentThread) {
        cpu->currentThread->parent->activeTicks++;
        if (cpu->currentThread->timeSlice > 0) {
            cpu->currentThread->timeSlice--;
            return;
        }
    }

    while (__builtin_expect(acquireTestLock(&cpu->runQueueLock), 0)) {
        return;
    }

    if (__builtin_expect(cpu->runQueue->get_length() <= 0 || !cpu->currentThread, 0)) {
        cpu->currentThread = cpu->idleThread;
    } else {
        if (__builtin_expect(cpu->currentThread->state == ThreadStateDying, 0)) {
            cpu->runQueue->remove(cpu->currentThread);
            cpu->currentThread = cpu->idleThread;
        } else if (__builtin_expect(cpu->currentThread->parent != cpu->idleProcess, 1)) {
            cpu->currentThread->timeSlice = cpu->currentThread->timeSliceDefault;

            asm volatile("fxsave64 (%0)" ::"r"((uintptr_t)cpu->currentThread->fxState) : "memory");

            cpu->currentThread->registers = *r;

            cpu->currentThread = cpu->currentThread->next;
        } else {
            cpu->currentThread = cpu->runQueue->front;
        }

        if (cpu->currentThread->state == ThreadStateBlocked) {
            Thread* first = cpu->currentThread;

            do {
                cpu->currentThread = cpu->currentThread->next;
            } while (cpu->currentThread->state == ThreadStateBlocked && cpu->currentThread != first);

            if (cpu->currentThread->state == ThreadStateBlocked) {
                cpu->currentThread = cpu->idleThread;
            }
        }
    }

    releaseLock(&cpu->runQueueLock);

    asm volatile("fxrstor64 (%0)" ::"r"((uintptr_t)cpu->currentThread->fxState) : "memory");

    asm volatile("wrmsr" ::"a"(cpu->currentThread->fsBase & 0xFFFFFFFF) /*Value low*/,
                 "d"((cpu->currentThread->fsBase >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000100) /*Set FS Base*/);

    TSS::SetKernelStack(&cpu->tss, (uintptr_t)cpu->currentThread->kernelStack);

    // Check for a few things
    // - Process is in usermode
    // - Pending unmasked signals
    // If true, invoke the signal handler
    if ((cpu->currentThread->registers.cs & 0x3) &&
        (cpu->currentThread->pendingSignals & ~cpu->currentThread->signalMask)) {
        if (cpu->currentThread->parent->State() == ThreadStateRunning) {
            int ret = acquireTestLock(&cpu->currentThread->lock);
            assert(!ret);

            cpu->currentThread->HandlePendingSignal(&cpu->currentThread->registers);
            releaseLock(&cpu->currentThread->lock);
        }
    }

    TaskSwitch(&cpu->currentThread->registers, cpu->currentThread->parent->GetPageMap()->pml4Phys);
}

} // namespace Scheduler
