#pragma once

#include <stddef.h>

#include <le/fn.h>

struct BootModule {
    void *base;
    size_t size;
    char *path;
    char *cmd_line;
};

namespace hal::boot {

void enumerate_boot_modules(Fn<BootModule *> callback);

}
