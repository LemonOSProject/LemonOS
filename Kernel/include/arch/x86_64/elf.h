#pragma once

#include <stdint.h>

typedef struct{
	unsigned char id[16];
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	uint64_t entry;
	uint64_t phOff;
	uint64_t shOff;
	uint32_t flags;
	uint16_t hdrSize;
	uint16_t phEntrySize;
	uint16_t phNum;
	uint16_t shEntrySize;
	uint16_t shNum;
	uint16_t shStrIndex;
} __attribute__((packed)) elf64_header_t;

typedef struct{
	uint32_t type;
	uint32_t flags;
	uint64_t offset;
	uint64_t vaddr;
	uint64_t paddr;
	uint64_t fileSize;
	uint64_t memSize;
	uint64_t align;
} __attribute__((packed)) elf64_program_header_t;

typedef struct {
	uint64_t entry;
	uint64_t pHdrSegment;
	uint64_t phEntrySize;
	uint64_t phNum;

	char* linkerPath;
} elf_info_t;

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6

typedef struct process process_t;

int VerifyELF(void* elf);
elf_info_t LoadELFSegments(process_t* proc, void* elf, uintptr_t base);