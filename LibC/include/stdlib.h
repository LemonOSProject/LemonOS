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
int atoi(const char*);
long int atol(const char* str);

#warning "atexit and getenv not implemented"
// Not implemented
int atexit(void (*)(void));
char* getenv(const char*);

void exit();

#ifdef __cplusplus
}
#endif

#endif 
