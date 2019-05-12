#ifndef UNISTD_H
#define UNISTD_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#warning "Nothing here implemented"

int execv(const char*, char* const[]);
int execve(const char*, char* const[], char* const[]);
int execvp(const char*, char* const[]);
pid_t fork(void);

#ifdef __cplusplus
}
#endif

#endif