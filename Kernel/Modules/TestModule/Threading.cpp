#include <Scheduler.h>

static void ProcessEntry(){
    void* buf = kmalloc(4096);

    memset(buf, 1, 4096);

    acquireLock(&Scheduler::GetCurrentThread()->lock);
    Scheduler::EndProcess(Scheduler::GetCurrentProcess());
}

int ThreadingTest(){
    Process* processes[40];
    Process* proc = Scheduler::GetCurrentProcess();

    for(unsigned i = 0; i < 40; i++){
        processes[i] = Scheduler::CreateProcess((void*)ProcessEntry);

        processes[i]->parent = proc;
        proc->children.add_back(proc);

        Log::Info("[TestModule] Created process (PID %d)", processes[i]->pid);
    }

    while(proc->children.get_length()){
        for(Process* child : proc->children){
            if(child->isDead){
                Log::Info("[TestModule] Killed process (PID %d)", child->pid);
                proc->RemoveChild(child);
            }
        }
    }

    return 0;
}