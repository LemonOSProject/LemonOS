#include <cpuid.h>

namespace CPU{
	cpuid_info_t GetCPUInfo() {
		cpuid_info_t cpuid_info_s;
		uint32_t ebx;
		uint32_t edx;
		uint32_t ecx;

		asm volatile("cpuid": "=b"(ebx),"=d"(edx),"=c"(ecx) : "a"(0)); // Get vendor string
		for (int i = 0; i < 4; i++) cpuid_info_s.vendor_string[i] = ebx >> (i * 8) & 0xFF; // Copy string to buffer
		for (int i = 0; i < 4; i++) cpuid_info_s.vendor_string[i+4] = edx >> (i * 8) & 0xFF;
		for (int i = 0; i < 4; i++) cpuid_info_s.vendor_string[i+8] = ecx >> (i * 8) & 0xFF;

		asm volatile("cpuid": "=d"(edx), "=c"(ecx) : "a"(1)); // Get features
		cpuid_info_s.features_ecx = ecx;
		cpuid_info_s.features_edx = edx;
		return cpuid_info_s;
	}
}