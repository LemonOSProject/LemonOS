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

// When the run queue was last balanced
uint64_t nextBalanceDue = 0;

void Schedule(void*, RegisterContext* r);

void InsertNewThreadIntoQueue(Thread* thread) {
    CPU* cpu = SMP::cpus[0];
    for (unsigned i = 1; i < SMP::processorCount; i++) {
        // Pick the CPU with the most idle time since run queue balance
        if (SMP::cpus[i]->idleProcess->GetMainThread()->ticksSinceBalance >
            cpu->idleProcess->GetMainThread()->ticksSinceBalance) {
            cpu = SMP::cpus[i];
        }

        // Pick a CPU with an empty run queue no matter what
        if (!cpu->runQueue->get_length()) {
            break;
        }
    }

    asm("sti");
    acquireLockIntDisable(&cpu->runQueueLock);
    cpu->runQueue->add_back(thread);
    thread->cpu = cpu->id;
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

void BalanceRunQueues() {
    assert(processesLock);
    assert(!CheckInterrupts());

    if(Timer::UsecondsSinceBoot() - nextBalanceDue > 1000000) {
        Log::Warning("Exceeded run balance due time by %d", Timer::UsecondsSinceBoot() - nextBalanceDue);
    }

    long avgUtilization[SMP::processorCount];
    long leastActive = 0;

    FastList<Thread*> threadsToDistribute;

    // Lock ALL run queues
    for (unsigned i = 0; i < SMP::processorCount; i++) {
        CPU* cpu = SMP::cpus[i];
        acquireLock(&cpu->runQueueLock);

        if (!cpu->currentThread || cpu->currentThread == cpu->idleThread) {
            avgUtilization[i] = 0;
        } else {
            // TODO: Move the thread even if the CPU was using it
            // TODO: Consider leaving high utilization processes on the CPU
            // as they have likely populated the CPU cache
            if (cpu->currentThread->ticksGivenSinceBalance) {
                avgUtilization[i] =
                    cpu->currentThread->ticksSinceBalance * 100 / cpu->currentThread->ticksGivenSinceBalance;
            } else {
                avgUtilization[i] = 0;
            }
        }

        if (cpu->runQueue->get_length()) {
        retry:
            Thread* it = cpu->runQueue->get_front();
            do {
                if (!it) {
                    break;
                }

                if (it != cpu->currentThread) {
                    // Save next thread as it will be overwritten by
                    // intrusive list
                    cpu->runQueue->remove(it);
                    it->cpu = -1;
                    threadsToDistribute.add_back(it);

                    goto retry; // The front may have changed
                } else {
                    it = it->next;
                }
            } while (it != cpu->runQueue->get_front());
        }
    }

    Thread* nextThread;
    while ((nextThread = threadsToDistribute.get_front())) {
        CPU* cpu = SMP::cpus[leastActive];

        threadsToDistribute.remove(nextThread);
        cpu->runQueue->add_back(nextThread);
        nextThread->cpu = cpu->id;

        Log::Debug(debugLevelScheduler, DebugLevelVerbose, "Gave %s to CPU %d", nextThread->parent->name, leastActive);

        if (nextThread->ticksGivenSinceBalance > 0) {
            avgUtilization[leastActive] += nextThread->ticksSinceBalance * 100 / nextThread->ticksGivenSinceBalance;
        }

        if (!threadsToDistribute.get_front()) {
            break; // Done distributing threads
        }

        for (unsigned i = 0; i < SMP::processorCount; i++) {
            Log::Debug(debugLevelScheduler, DebugLevelVerbose, "util: %d, %d", i, avgUtilization[i]);

            if (avgUtilization[i] < avgUtilization[leastActive]) {
                leastActive = i;
            }
        }
    }

    // Unlock ALL run queues
    for (unsigned i = 0; i < SMP::processorCount; i++) {
        Thread* thread = SMP::cpus[i]->runQueue->get_front();
        if (thread) {
            do {
                thread->ticksSinceBalance = 0;
                thread->ticksGivenSinceBalance = 0;

                Log::Debug(debugLevelScheduler, DebugLevelVerbose, "%d: resetting %s (pid %d tid %d)", i,
                           thread->parent->name, thread->parent->PID(), thread->tid);
                thread = thread->next;
            } while (thread != SMP::cpus[i]->runQueue->get_front());
        }

        SMP::cpus[i]->idleThread->ticksSinceBalance = 0;

        releaseLock(&SMP::cpus[i]->runQueueLock);
    }

    nextBalanceDue = Timer::UsecondsSinceBoot() + 1000000;
}

void Schedule(__attribute__((unused)) void* data, RegisterContext* r) {
    CPU* cpu = GetCPULocal();

    if (Timer::UsecondsSinceBoot() > nextBalanceDue) {
        if(!__builtin_expect(acquireTestLock(&processesLock), 0)) {
            if (Timer::UsecondsSinceBoot() > nextBalanceDue) {
                BalanceRunQueues();
            }
            releaseLock(&processesLock);
        }
    }

    if (cpu->currentThread) {
        cpu->currentThread->parent->activeTicks++;
        if (cpu->currentThread->timeSlice > 0 && !(cpu->currentThread->state & ThreadStateBlocked)) {
            cpu->currentThread->ticksSinceBalance++;
            cpu->currentThread->timeSlice--;
            return;
        }
    }

    while (__builtin_expect(acquireTestLock(&cpu->runQueueLock), 0)) {
        asm volatile("pause");
    }

    if (__builtin_expect(cpu->runQueue->get_length() <= 0 || !cpu->currentThread, 0)) {
        cpu->currentThread = cpu->idleThread;
    } else if (__builtin_expect(cpu->currentThread->state == ThreadStateDying, 0)) {
        cpu->runQueue->remove(cpu->currentThread);
        cpu->currentThread->cpu = -1;
        cpu->currentThread = cpu->idleThread;
    } else {
        if (__builtin_expect(cpu->currentThread->parent != cpu->idleProcess, 1)) {
            cpu->currentThread->timeSlice = cpu->currentThread->timeSliceDefault;
            cpu->currentThread->ticksGivenSinceBalance += cpu->currentThread->timeSliceDefault;

            asm volatile("fxsave64 (%0)" ::"r"((uintptr_t)cpu->currentThread->fxState) : "memory");

            cpu->currentThread->registers = *r;

            cpu->currentThread = cpu->currentThread->next;
        } else {
            cpu->currentThread = cpu->runQueue->front;
        }

        // Get the next thread that isnt blocked
        if (cpu->currentThread->state & ThreadStateBlocked) {
            Thread* first = cpu->currentThread;

            do {
                cpu->currentThread = cpu->currentThread->next;
            } while ((cpu->currentThread->state & ThreadStateBlocked) && cpu->currentThread != first);
        }

        // Check if we could find an unblocked thread
        if (cpu->currentThread->state & ThreadStateBlocked) {
                cpu->currentThread = cpu->idleThread;
        } else {
            // See if we can find a better thread
            // where better is the thread w/ least CPU time
            Thread* leastHungry = cpu->currentThread->next;
            Thread* first = cpu->currentThread;
            do {
                if (leastHungry->state & ThreadStateBlocked) {
                    Thread* first = leastHungry;

                    do {
                        leastHungry = leastHungry->next;
                    } while ((leastHungry->state & ThreadStateBlocked) && leastHungry != first);

                    if (leastHungry->state & ThreadStateBlocked) {
                        cpu->currentThread = cpu->idleThread;
                        break;
                    }
                }

                if(leastHungry == first) {
                    break;
                }

                if(leastHungry->ticksSinceBalance < cpu->currentThread->ticksSinceBalance) {
                    cpu->currentThread = leastHungry;
                }

                leastHungry = leastHungry->next;
                //Log::Info("pid %d (c: %d)", leastHungry->parent->PID(), cpu->currentThread->parent->PID());
            } while(leastHungry != first);
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
        (cpu->currentThread->pendingSignals & ~cpu->currentThread->EffectiveSignalMask())) {
        if (cpu->currentThread->parent->State() == Process::Process_Running) {
            int ret = acquireTestLock(&cpu->currentThread->kernelLock);
            assert(!ret);

            cpu->currentThread->HandlePendingSignal(&cpu->currentThread->registers);
            releaseLock(&cpu->currentThread->kernelLock);
        }
    }

    asm volatile(
        R"(mov %0, %%rsp;
        mov %1, %%rax;
        pop %%r15;
        pop %%r14;
        pop %%r13;
        pop %%r12;
        pop %%r11;
        pop %%r10;
        pop %%r9;
        pop %%r8;
        pop %%rbp;
        pop %%rdi;
        pop %%rsi;
        pop %%rdx;
        pop %%rcx;
        pop %%rbx;
        
        mov %%rax, %%cr3
        pop %%rax
        addq $8, %%rsp
        iretq)" ::"r"(&cpu->currentThread->registers),
        "r"(cpu->currentThread->parent->GetPageMap()->pml4Phys));
}

} // namespace Scheduler
