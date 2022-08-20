#include <Objects/KObject.h> 

void KernelObject::Watch(KernelObjectWatcher& watcher, int events){
    watcher.Signal();
}

void KernelObject::Unwatch(KernelObjectWatcher& watcher){
    (void)watcher;
}
