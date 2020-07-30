#pragma once

typedef struct {
	uint64_t totalMem;
	uint64_t usedMem;
	uint16_t cpuCount;
} lemon_sysinfo_t;

namespace Lemon{
	extern char* versionString;
}