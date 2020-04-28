#include <cpuid.h>

namespace CPU{
	cpuid_info_t CPUID() {
		cpuid_info_t info;
		uint32_t ebx;
		uint32_t edx;
		uint32_t ecx;

		asm volatile("cpuid": "=b"(ebx),"=d"(edx),"=c"(ecx) : "a"(0)); // Get vendor string
		for (int i = 0; i < 4; i++) info.vendorString[i] = ebx >> (i * 8) & 0xFF; // Copy string to buffer
		for (int i = 0; i < 4; i++) info.vendorString[i+4] = edx >> (i * 8) & 0xFF;
		for (int i = 0; i < 4; i++) info.vendorString[i+8] = ecx >> (i * 8) & 0xFF;

		asm volatile("cpuid": "=d"(edx), "=c"(ecx) : "a"(1)); // Get features
		info.features_ecx = ecx;
		info.features_edx = edx;
		return info;
	}
}