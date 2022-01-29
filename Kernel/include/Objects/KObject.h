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

#define KOBJECT_ID_MESSAGE_ENDPOINT 1
#define KOBJECT_ID_INTERFACE 2
#define KOBJECT_ID_SERVICE 3
#define KOBJECT_ID_UNIX_OPEN_FILE 4
#define KOBJECT_ID_PROCESS 5

class KernelObjectWatcher;

class KernelObject {
protected:
    int64_t oid = -1;
    static int64_t nextOID;

public:
    KernelObject() { oid = nextOID++; }

    inline int64_t ObjectID() { return oid; }

    virtual kobject_id_t InstanceTypeID() const = 0;

    inline bool IsType(kobject_id_t id) { return InstanceTypeID() == id; }

    virtual void Watch(KernelObjectWatcher& watcher, int events);
    virtual void Unwatch(KernelObjectWatcher& watcher);

    virtual void Destroy() = 0;

    virtual ~KernelObject() = default;
};

class KernelObjectWatcher : public Semaphore {
    List<FancyRefPtr<KernelObject>> watching;

public:
    KernelObjectWatcher() : Semaphore(0) {}

    inline void WatchObject(FancyRefPtr<KernelObject> node, int events) {
        node->Watch(*this, events);

        watching.add_back(node);
    }

    ~KernelObjectWatcher() {
        for (auto& node : watching) {
            node->Unwatch(*this);
        }

        watching.clear();
    }
};
