#ifndef UNISTD_H
#define UNISTD_H

#include <sys/types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#warning "Nothing here implemented"

int execv(const char*, char* const[]);
int execve(const char*, char* const[], char* const[]);
int execvp(const char*, char* const[]);
/*pid_t*/uint32_t fork(void);

/*off_t*/uint32_t lseek(int fd, /*off_t*/uint32_t offset, int whence);

size_t read(int fd, void* buffer, size_t count);
size_t write(int fd, void* buffer, size_t count);
int access(const char* filename, int mode);

int usleep(unsigned seconds);

#ifdef __cplusplus
}
#endif

#endif