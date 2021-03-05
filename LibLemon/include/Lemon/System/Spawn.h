#pragma once

#ifndef __lemon__
    #error "Lemon OS Only"
#endif

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>

#define EXEC_CHILD 1

pid_t lemon_spawn(const char* path, int argc, char* const argv[], int flags = 0);
pid_t lemon_spawn(const char* path, int argc, char* const argv[], int flags, char** envp);