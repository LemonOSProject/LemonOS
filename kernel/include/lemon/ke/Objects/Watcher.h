#pragma once

#include <Objects/KObject.h>

#include <List.h>
#include <Lock.h>

// KernelObjectWatcher is a semaphore initialized to 0.
// A thread can wait on it like any semaphore,
// and when a file is ready it will signal and waiting thread(s) will get woken
class KernelObjectWatcher : public Semaphore {
    List<FancyRefPtr<KernelObject>> watching;

public:
    KernelObjectWatcher() : Semaphore(0) {}

    template <KernelObjectDerived O> ALWAYS_INLINE void Watch(FancyRefPtr<O> object, KOEvent events) {
        object->Watch(this, events);

        watching.add_back(static_pointer_cast<KernelObject, O>(std::move(object)));
    }

    ~KernelObjectWatcher() {
        for (const auto& f : watching) {
            f->Unwatch(this);
        }
    }
};
