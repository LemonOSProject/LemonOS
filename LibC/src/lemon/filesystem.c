#include <lemon/syscall.h>
#include <stddef.h>
#include <lemon/filesystem.h>

int lemon_open(char* filename, int flags){
    int fd;
    syscall(SYS_OPEN, filename, &fd, 0, 0, 0);
    return fd;
}

void lemon_close(int fd){
    syscall(SYS_CLOSE, fd, 0, 0, 0, 0);
}

int lemon_read(int fd, void* buffer, size_t count){
    int ret;
    syscall(SYS_READ, fd, buffer, count, &ret, 0);
    return ret;
}

int lemon_write(int fd, void* buffer, size_t count){
    int ret;
    syscall(SYS_WRITE, fd, buffer, count, &ret, 0);
    return ret;
}

off_t lemon_seek(int fd, off_t offset, int whence){
    uint64_t ret;
    syscall(SYS_LSEEK, fd, offset, whence, &ret, 0);
    return ret;
}

int lemon_readdir(int fd, uint32_t count, lemon_dirent_t* dirent){
    int ret;
    syscall(SYS_READDIR, fd, dirent, count, &ret, 0);
    return ret;
}

size_t read(int fd, void* buffer, size_t count){
    return lemon_read(fd, buffer, count);
}

size_t write(int fd, void* buffer, size_t count){
    return lemon_write(fd, buffer, count);
}

off_t lseek(int fd, off_t offset, int whence){
    return lemon_seek(fd, offset, whence);
}

int open(const char* filename, int flags){
    return lemon_open(filename, flags);
}

int close(int fd){
    lemon_close(fd);
    return 0;
}

#warning Properly Implement access()
int access(const char* filename, int mode){
    int ret = lemon_open(filename, 0);

    if(ret) {
        lemon_close(ret);
        return 0;
    } else return 1;
}