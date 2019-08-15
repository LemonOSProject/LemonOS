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
#define SYS_MSG_SEND 14
#define SYS_MSG_RECIEVE 15
#define SYS_UNAME 16
#define SYS_MEMALLOC 17
#define SYS_STAT 18
#define SYS_LSEEK 19
#define SYS_GETPID 20
#define SYS_MOUNT 21
#define SYS_UMOUNT 22
#define SYS_CREATE_DESKTOP 23
#define SYS_CREATE_WINDOW 24
#define SYS_PAINT_DESKTOP 25
#define SYS_PAINT_WINDOW 26
#define SYS_DESKTOP_GET_WINDOW 27
#define SYS_DESKTOP_GET_WINDOW_COUNT 28
#define SYS_COPY_WINDOW_SURFACE 29
#define SYS_READDIR 30
#define SYS_UPTIME 31
#define SYS_GETVIDEOMODE 32
#define SYS_DESTROY_WINDOW 33
#define SYS_GRANTPTY 34
#define SYS_FORKEXEC 35
#define SYS_GET_SYSINFO 36

#ifdef __cplusplus
extern "C"{
#endif
void syscall(uint32_t call, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);

#ifdef __cplusplus
}
#endif

#endif
