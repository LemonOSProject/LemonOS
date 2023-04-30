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
    static constexpr KOTypeID type_id() { return KOTypeID::type; }                                                     \
    KOTypeID instance_type_id() const final override { return type_id(); }                                             \
                                                                                                                       \
protected:
// define DECLARE_KOBJECT(type)

class KernelObject {
public:
    virtual KOTypeID instance_type_id() const = 0;

    inline bool is_type(KOTypeID id) {
        return instance_type_id() == id;
    }

    virtual KOEvent poll_events(KOEvent mask);

    virtual void Watch(class KernelObjectWatcher* watcher, KOEvent events);
    virtual void Unwatch(class KernelObjectWatcher* watcher);

    virtual ~KernelObject() = default;
};

template <typename T>
concept KernelObjectDerived = requires(T t) {
                                  T::type_id();
                                  t.instance_type_id();
                                  static_cast<KernelObject*>(&t);
                              };
