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
int strcmp(char* s1, char* s2);

char *strtok(char * str, const char * delim);

int strlen(const char* str);
char* strcat(char* dest, const char* src);