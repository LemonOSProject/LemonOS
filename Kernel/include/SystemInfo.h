#include <stdint.h>

typedef struct {

	char cpuVendor[12];
	char null = '\0';
	char versionString[64];

	uint32_t memSize;
	uint32_t usedMemory;
} sys_info_t;