#ifndef LEMON_H
#define LEMON_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdint.h>

typedef struct {

	char cpuVendor[12];
	char null;
	char versionString[64];

	uint32_t memSize;
	uint32_t usedMemory;
} lemon_sys_info_t;

char* lemon_uname(char* string);
void lemon_sysinfo(lemon_sys_info_t* sys);

#ifdef __cplusplus
}
#endif

#endif