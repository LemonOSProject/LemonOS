#pragma once

#ifndef __lemon__
    #error "Lemon OS Only"
#endif

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>

pid_t lemon_spawn(const char* path, int argc, char** argv, int flags = 0);