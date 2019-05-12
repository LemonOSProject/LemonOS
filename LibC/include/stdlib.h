#ifndef STDLIB_H
#define STDLIB_H

#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void*)0)
#endif

#include <liballoc.h>

void abort();

#warning "atexit, atoi and getenv not implemented"
// Not implemented
int atexit(void (*)(void));
int atoi(const char*);
char* getenv(const char*);

#endif 
