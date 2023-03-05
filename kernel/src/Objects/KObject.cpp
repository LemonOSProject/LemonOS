#include <Objects/KObject.h>

void KernelObject::Watch(KernelObjectWatcher* watcher, KOEvent) { watcher->Signal(); }

void KernelObject::Unwatch(KernelObjectWatcher*) {}
