#include <lemon/syscall.h>
#include <stddef.h>
#include <lemon/filesystem.h>

int lemon_open(const char* filename, int flags){
    return syscall(SYS_OPEN, (uintptr_t)filename, 0, 0, 0, 0);
}

void lemon_close(int fd){
    syscall(SYS_CLOSE, fd, 0, 0, 0, 0);
}

int lemon_read(int fd, void* buffer, size_t count){
    return syscall(SYS_READ, fd, (uintptr_t)buffer, count, 0, 0);
}

int lemon_write(int fd, const void* buffer, size_t count){
    return syscall(SYS_WRITE, fd, (uintptr_t)buffer, count, 0, 0);
}

off_t lemon_seek(int fd, off_t offset, int whence){
    return syscall(SYS_LSEEK, fd, offset, whence, 0, 0);
}

int lemon_readdir(int fd, uint64_t count, lemon_dirent_t* dirent){
    int ret = syscall(SYS_READDIR, fd, (uintptr_t)dirent, count, 0, 0);
    if(ret < 0)
        return 0;
    else return 1;
} 
