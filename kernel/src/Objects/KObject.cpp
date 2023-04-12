#include <Objects/KObject.h>

#include <Objects/Watcher.h>

void KernelObject::Watch(KernelObjectWatcher* watcher, KOEvent) { watcher->Signal(); }

void KernelObject::Unwatch(KernelObjectWatcher*) {}
