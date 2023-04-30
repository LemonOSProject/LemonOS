#pragma once

#include <stdint.h>

extern "C" {

// Returns 1 on fault
int user_memcpy(void* dest, const void* src, size_t count);
void user_memcpy_trap();
void user_memcpy_trap_handler();

long user_strnlen(const char* str, size_t maxLength);
void user_strlen_trap();
void user_strlen_trap_handler();

long user_memset(void* dest, int c, size_t count);
void user_memset_trap();
void user_memset_trap_handler();

int user_store64(void* dest, uint64_t value);
void user_store64_trap();
void user_store64_trap_handler();

}
