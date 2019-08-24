#pragma once

#include <stddef.h>

char* itoa(unsigned long long num, char* str, int base);

extern "C"
void* memset(void* src, int c, size_t count);
void *memcpy(void* dest, void* src, size_t count);
void memcpy_optimized(void* dest, void* src, size_t count);

void strcpy(char* dest, const char* src);
int strcmp(char* s1, char* s2);

char *strtok(char * str, const char * delim);

int strlen(const char* str);
char* strcat(char* dest, const char* src);