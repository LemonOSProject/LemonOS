#pragma once

#include <stdint.h>

typedef struct {
	uint64_t totalMem;
	uint64_t usedMem;
} lemon_sysinfo_t;