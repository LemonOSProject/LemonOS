#pragma once

#include <Debug.h>
#ifdef DEBUG_KOBJECTS
#include <Logging.h>
#endif

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
    static constexpr KOTypeID TypeID() { return KOTypeID::type; }                                                    \
    KOTypeID InstanceTypeID() const final override { return TypeID(); }                                                \
                                                                                                                       \
protected:

class KernelObjectWatcher;

class KernelObjectWatcher : public Semaphore {

};

class KernelObject {
public:
    virtual KOTypeID InstanceTypeID() const = 0;

    inline bool IsType(KOTypeID id) { return InstanceTypeID() == id; }

    virtual void Watch(KernelObjectWatcher& watcher, int events);
    virtual void Unwatch(KernelObjectWatcher& watcher);

    virtual ~KernelObject() = default;
};

template<typename T>
concept KernelObjectDerived = requires (T t) {
    T::TypeID();
    t.InstanceTypeID();
    static_cast<KernelObject*>(&t);
};
