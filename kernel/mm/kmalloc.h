#pragma once

#include <stdint.h>
#include <stddef.h>

#include <le/intrusive_list.h>

namespace mm {

/**
 * @brief  Allocates memory of size 'sz'
 * 
 * @param sz - size of allocation, must be at least 8 and no more than 256 bytes
 * @return pointer to newly allocated memory
*/
void *kmalloc(size_t sz);

/**
 * @brief Frees memory allocated by kmalloc
 * 
 * @param ptr 
*/
void kfree(void *ptr);

void kmalloc_test();

}
