#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_EXIT 1
#define SYS_EXEC 2
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_SLEEP 7
#define SYS_CREATE 8
#define SYS_LINK 9
#define SYS_UNLINK 10
#define SYS_CHDIR 12
#define SYS_TIME 13
#define SYS_MAP_FB 14

#ifdef __cplusplus
extern "C"{
#endif

__attribute__((weak))
void syscall(uint64_t call, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);

#ifdef __cplusplus
}
#endif

#endif
