#pragma once

#include <Debug.h>
#ifdef DEBUG_KOBJECTS
#include <Logging.h>
#endif

#include <Objects/Event.h>

#include <CString.h>
#include <Lock.h>
#include <RefPtr.h>
#include <stdint.h>

typedef long kobject_id_t;

enum class KOTypeID : int {
    Invalid,
    MessageEndpoint,
    MessageInterface,
    Service,
    File,
    Process,
};

#define DECLARE_KOBJECT(type)                                                                                          \
public:                                                                                                                \
    static constexpr KOTypeID TypeID() { return KOTypeID::type; }                                                      \
    KOTypeID InstanceTypeID() const final override { return TypeID(); }                                                \
                                                                                                                       \
protected:
// define DECLARE_KOBJECT(type)

class KernelObject {
public:
    virtual KOTypeID InstanceTypeID() const = 0;

    inline bool IsType(KOTypeID id) { return InstanceTypeID() == id; }

    virtual void Watch(class KernelObjectWatcher* watcher, KOEvent events);
    virtual void Unwatch(class KernelObjectWatcher* watcher);

    virtual ~KernelObject() = default;
};

template <typename T>
concept KernelObjectDerived = requires(T t) {
    T::TypeID();
    t.InstanceTypeID();
    static_cast<KernelObject*>(&t);
};

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
