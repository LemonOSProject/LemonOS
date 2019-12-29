#ifndef FCNTL_H
#define FCNTL_H

#define F_OK 0
#define R_OK 4

#define O_RDONLY  00
#define O_WRONLY  01
#define O_RDWR    02

#ifdef __cplusplus
extern "C"{
#endif

#include <stddef.h>

int open(const char* filename, int flags);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
size_t write(int fd, void* buffer, size_t count);
size_t read(int fd, void* buffer, size_t count);

#ifdef __cplusplus
}
#endif

#endif