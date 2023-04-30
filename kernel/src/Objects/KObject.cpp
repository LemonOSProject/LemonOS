#include <Objects/KObject.h>

#include <Objects/Watcher.h>

KOEvent KernelObject::poll_events(KOEvent) {
    return KOEvent::None;
}

void KernelObject::Watch(KernelObjectWatcher* watcher, KOEvent) {
    watcher->signal();
}

void KernelObject::Unwatch(KernelObjectWatcher*) {
}
