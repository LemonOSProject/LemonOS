#ifndef STRING_H
#define STRING_H

#include <stddef.h>

#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void*)0)
#endif

#ifdef __cplusplus
extern "C"{
#endif

void* memset(void* src, int c, size_t count);
void *memcpy(void* dest, void* src, size_t count);
void *memmove(void* dest, void* src, size_t count);
void* memchr(const void* src, int c, size_t count);
int memcmp(const void* ptr1, const void* ptr2, size_t count);

int strlen(const char* str);
char* strcat(char* dest, const char* src);
char* strchr(const char *s, int c);
size_t strspn(const char *s1, const char *s2);
size_t strcspn(const char *s1, const char *s2);
char* strtok(char * str, const char * delim);
char* strcpy(char* dest, const char* src);
int strcmp(const char *str1, const char *str2);
void strncpy(char* dest, const char* src, size_t n);

#ifdef __cplusplus
}
#endif

#endif