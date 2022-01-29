#pragma once

#include <Objects/KObject.h>
#include <RefPtr.h>
#include <Compiler.h>

typedef int handle_id_t;

struct Handle {
    handle_id_t id = 0;
    FancyRefPtr<KernelObject> ko;

    ALWAYS_INLINE operator bool(){
        return (id > 0) && ko.get();
    }

    bool closeOnExec : 1; // O_CLOEXEC POSIX flag
};

#define HANDLE_NULL ((Handle){-1, nullptr});
