#ifndef FS_H
#define FS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

int lemon_open(char* filename, int flags);
void lemon_close(int fd);
void lemon_read(int fd, void* buffer, size_t count);
void lemon_write(int fd, void* buffer, size_t count);

#ifdef __cplusplus
}
#endif

#endif 
