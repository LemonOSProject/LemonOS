#pragma once

#include <Objects/KObject.h>
#include <RefPtr.h>
#include <Move.h>
#include <Compiler.h>

#include <cstddef>

typedef int handle_id_t;

struct Handle {
    handle_id_t id = -1;
    FancyRefPtr<KernelObject> ko = nullptr;

    ALWAYS_INLINE bool IsValid() const {
        return ko;
    }

    ALWAYS_INLINE operator bool() const {
        return IsValid();
    }

    bool closeOnExec : 1 = false; // O_CLOEXEC POSIX flag
};

ALWAYS_INLINE Handle MakeHandle(handle_id_t id, KernelObject* ko) {
    return Handle{id, FancyRefPtr<KernelObject>(ko), false};
}

#define HANDLE_NULL ((Handle){-1, nullptr, false})

ALWAYS_INLINE bool operator==(const Handle& l, const Handle& r) {
    return (l.id == r.id);
}

ALWAYS_INLINE bool operator==(const Handle& l, std::nullptr_t) {
    return (l.id == -1);
}
