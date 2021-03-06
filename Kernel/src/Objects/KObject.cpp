#include <Objects/KObject.h> 

int64_t KernelObject::nextOID = 2; 

void KernelObject::Watch(KernelObjectWatcher& watcher, int events){
    watcher.Signal();
}

void KernelObject::Unwatch(KernelObjectWatcher& watcher){
    (void)watcher;
}