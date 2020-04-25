#ifndef SPAWN_H
#define SPAWN_H

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

pid_t lemon_spawn(const char* path, int argc, char** argv, int flags = 0);

#ifdef __cplusplus
}
#endif

#endif 
