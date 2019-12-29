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
} elf64_header_t;

typedef struct{
	uint32_t type;
	uint32_t flags;
	uint64_t offset;
	uint64_t vaddr;
	uint64_t paddr;
	uint64_t fileSize;
	uint64_t memSize;
	uint64_t align;
} elf64_program_header_t;