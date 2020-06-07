#pragma once

#ifndef __lemon__
    #error "Lemon OS Only"
#endif

#include <stdint.h>

typedef struct {
	uint64_t totalMem;
	uint64_t usedMem;
} lemon_sysinfo_t;

lemon_sysinfo_t lemon_sysinfo();