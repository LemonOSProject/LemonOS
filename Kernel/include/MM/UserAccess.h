#pragma once

#include <stdint.h>

extern "C" {

int user_memcpy(void* dest, const void* src, size_t count);
void user_memcpy_trap();
void user_memcpy_trap_handler();

long user_strlen(const char* str);
void user_strlen_trap();
void user_strlen_trap_handler();

}
