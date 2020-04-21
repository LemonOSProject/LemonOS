#pragma once

#include <stdint.h>

typedef struct{
	uint64_t a_type;
	uint64_t a_val;
} auxv_t;

// Auxillary Vectors
#define AT_NULL 0
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_ENTRY 9