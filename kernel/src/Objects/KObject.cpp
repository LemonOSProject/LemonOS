#include <Objects/KObject.h>

#include <Objects/Watcher.h>

void KernelObject::Watch(KernelObjectWatcher* watcher, KOEvent) { watcher->signal(); }

void KernelObject::Unwatch(KernelObjectWatcher*) {}
