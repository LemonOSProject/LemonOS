#pragma once

#include <stdint.h>

typedef struct{
	uint64_t a_type;
	uint64_t a_val;
} auxv_t;

// Auxillary Vectors
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_ENTRY 9
#define AT_EXECPATH 15
#define AT_RANDOM 25
#define AT_EXECFN 31
#define AT_SYSINFO_EHDR 33