#pragma once

#include <stddef.h>

char* itoa(unsigned long long num, char* str, int base);

extern "C"
void* memset(void* src, int c, size_t count);
extern "C"
void *memcpy(void* dest, const void* src, size_t count);
extern "C"
int memcmp(const void *s1, const void *s2, size_t n);

void memcpy_optimized(void* dest, void* src, size_t count);

void strcpy(char* dest, const char* src);
void strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);

char *strtok(char * str, const char * delim);

size_t strlen(const char* str);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);

int strncmp(const char* s1, const char* s2, size_t n);

char* strupr(char* s);
char* strnupr(char* s, size_t n);

char* strchr(const char *s, int c);
char* strrchr(const char *s, int c);

char* strdup(const char* s);