#include <core/syscall.h>
#include <stddef.h>

int lemon_open(char* filename, int flags){
    int fd;
    syscall(SYS_OPEN, (uint32_t)filename, (uint32_t)&fd, 0, 0, 0);
    return fd;
}

void lemon_close(int fd){
    syscall(SYS_CLOSE, fd, 0, 0, 0, 0);
}

void lemon_read(int fd, void* buffer, size_t count){
    syscall(SYS_READ, fd, buffer, count, 0, 0);
}

void lemon_write(int fd, void* buffer, size_t count){
    syscall(SYS_WRITE, fd, buffer, count, 0, 0);
}

int lemon_seek(int fd, uint32_t offset, int whence){
    uint32_t ret;
    syscall(SYS_LSEEK, fd, offset, whence, (uint32_t)&ret, 0);
    return ret;
}