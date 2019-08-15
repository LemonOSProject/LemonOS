#ifndef STDLIB_H
#define STDLIB_H

#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void*)0)
#endif

#ifdef __cplusplus
extern "C"{
#endif

#include <liballoc.h>

void abort();

char* itoa(long num, char* str, int base);

#warning "atexit, atoi and getenv not implemented"
// Not implemented
int atexit(void (*)(void));
int atoi(const char*);
char* getenv(const char*);

void exit();

#ifdef __cplusplus
}
#endif

#endif 
