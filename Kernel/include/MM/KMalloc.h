#pragma once

#include <stddef.h>

void* kmalloc(size_t);
void kfree(void*);
void* krealloc(void*, size_t);
