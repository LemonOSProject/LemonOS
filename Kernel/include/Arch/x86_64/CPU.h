#pragma once

#include <stdint.h>
#include <TSS.h>
#include <Thread.h>
#include <System.h>

struct Process;
template<typename T>
class FastList;

typedef struct {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

struct CPU{
	CPU* self;
    uint64_t id; // APIC/CPU id
    void* gdt; // GDT
	gdt_ptr_t gdtPtr;
	Thread* currentThread = nullptr;
	Process* idleProcess = nullptr;
	volatile int runQueueLock = 0;
	FastList<Thread*>* runQueue;
    tss_t tss __attribute__((aligned(16)));
};

enum {
	CPUID_ECX_SSE3 = 1 << 0,
	CPUID_ECX_PCLMUL = 1 << 1,
	CPUID_ECX_DTES64 = 1 << 2,
	CPUID_ECX_MONITOR = 1 << 3,
	CPUID_ECX_DS_CPL = 1 << 4,
	CPUID_ECX_VMX = 1 << 5,
	CPUID_ECX_SMX = 1 << 6,
	CPUID_ECX_EST = 1 << 7,
	CPUID_ECX_TM2 = 1 << 8,
	CPUID_ECX_SSSE3 = 1 << 9,
	CPUID_ECX_CID = 1 << 10,
	CPUID_ECX_FMA = 1 << 12,
	CPUID_ECX_CX16 = 1 << 13,
	CPUID_ECX_ETPRD = 1 << 14,
	CPUID_ECX_PDCM = 1 << 15,
	CPUID_ECX_PCIDE = 1 << 17,
	CPUID_ECX_DCA = 1 << 18,
	CPUID_ECX_SSE4_1 = 1 << 19,
	CPUID_ECX_SSE4_2 = 1 << 20,
	CPUID_ECX_x2APIC = 1 << 21,
	CPUID_ECX_MOVBE = 1 << 22,
	CPUID_ECX_POPCNT = 1 << 23,
	CPUID_ECX_AES = 1 << 25,
	CPUID_ECX_XSAVE = 1 << 26,
	CPUID_ECX_OSXSAVE = 1 << 27,
	CPUID_ECX_AVX = 1 << 28,

	CPUID_EDX_FPU = 1 << 0,
	CPUID_EDX_VME = 1 << 1,
	CPUID_EDX_DE = 1 << 2,
	CPUID_EDX_PSE = 1 << 3,
	CPUID_EDX_TSC = 1 << 4,
	CPUID_EDX_MSR = 1 << 5,
	CPUID_EDX_PAE = 1 << 6,
	CPUID_EDX_MCE = 1 << 7,
	CPUID_EDX_CX8 = 1 << 8,
	CPUID_EDX_APIC = 1 << 9,
	CPUID_EDX_SEP = 1 << 11,
	CPUID_EDX_MTRR = 1 << 12,
	CPUID_EDX_PGE = 1 << 13,
	CPUID_EDX_MCA = 1 << 14,
	CPUID_EDX_CMOV = 1 << 15,
	CPUID_EDX_PAT = 1 << 16,
	CPUID_EDX_PSE36 = 1 << 17,
	CPUID_EDX_PSN = 1 << 18,
	CPUID_EDX_CLF = 1 << 19,
	CPUID_EDX_DTES = 1 << 21,
	CPUID_EDX_ACPI = 1 << 22,
	CPUID_EDX_MMX = 1 << 23,
	CPUID_EDX_FXSR = 1 << 24,
	CPUID_EDX_SSE = 1 << 25,
	CPUID_EDX_SSE2 = 1 << 26,
	CPUID_EDX_SS = 1 << 27,
	CPUID_EDX_HTT = 1 << 28,
	CPUID_EDX_TM1 = 1 << 29,
	CPUID_EDX_IA64 = 1 << 30,
	CPUID_EDX_PBE = 1 << 31
};

typedef struct {
	char vendorString[12]; // CPU vendor string
	char nullTerminator = '\0'; // Acts as a terminator for the vendor string

	uint32_t features_ecx; // CPU features (ecx)
	uint32_t features_edx; // CPU features (edx)
} __attribute__((packed)) cpuid_info_t;

cpuid_info_t CPUID();

inline uintptr_t GetRBP(){
	volatile uintptr_t val;
	
	asm volatile("mov %%rbp, %0" : "=r"(val));
	return val;
}

inline uintptr_t GetCR3(){
	volatile uintptr_t val;
	
	asm volatile("mov %%cr3, %0" : "=r"(val));
	return val;
}

static inline void SetCPULocal(CPU* val){
	val->self = val;
	asm volatile("wrmsr" :: "a"((uintptr_t)val & 0xFFFFFFFF) /*Value low*/, "d"(((uintptr_t)val >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000102) /*Set Kernel GS Base*/);
	asm volatile("wrmsr" :: "a"((uintptr_t)val & 0xFFFFFFFF) /*Value low*/, "d"(((uintptr_t)val >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000101) /*Set Kernel GS Base*/);
}

__attribute__((always_inline)) static inline CPU* GetCPULocal(){
	CPU* ret;
	int intEnable = CheckInterrupts();
	asm("cli");
    asm volatile("swapgs; movq %%gs:0, %0; swapgs;" : "=r"(ret)); // CPU info is 16-byte aligned as per liballoc alignment
	if(intEnable)
		asm("sti");
	return ret;
}