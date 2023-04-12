#pragma once

#include <Objects/Event.h>

#include <Debug.h>
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
    Thread,
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
