#include <lemon/syscall.h>
#include <Lemon/System/Filesystem.h>
#include <stddef.h>

int lemon_open(const char* filename, int flags){
    return syscall(SYS_OPEN, (uintptr_t)filename, flags);
}

void lemon_close(int fd){
    syscall(SYS_CLOSE, fd);
}

int lemon_read(int fd, void* buffer, size_t count){
    return syscall(SYS_READ, fd, (uintptr_t)buffer, count);
}

int lemon_write(int fd, const void* buffer, size_t count){
    return syscall(SYS_WRITE, fd, (uintptr_t)buffer, count);
}

off_t lemon_seek(int fd, off_t offset, int whence){
    return syscall(SYS_LSEEK, fd, offset, whence);
}

int lemon_readdir(int fd, uint64_t count, lemon_dirent_t* dirent){
    return syscall(SYS_READDIR, fd, (uintptr_t)dirent, count);
} 
