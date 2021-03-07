#pragma once

#include <stdint.h>

typedef struct ELF64Header {
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

typedef struct ELF64ProgramHeader {
	uint32_t type;
	uint32_t flags;
	uint64_t offset;
	uint64_t vaddr;
	uint64_t paddr;
	uint64_t fileSize;
	uint64_t memSize;
	uint64_t align;
} __attribute__((packed)) elf64_program_header_t;

typedef struct ELF64DynamicEntry {
	int64_t tag; // Type of DT entry
	union {
		uint64_t val;
		uint64_t ptr;
	};
} __attribute__((packed)) elf64_dynamic_t;

typedef struct {
	uint64_t entry;
	uint64_t pHdrSegment;
	uint64_t phEntrySize;
	uint64_t phNum;

	char* linkerPath;
} elf_info_t;

#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PTRELSZ 2 // Size of the PLT relocaiton entries
#define DT_PLTGOT 3 // Address associated with the PLT
#define DT_HASH 4 // Symbol hashtable address
#define DT_STRTAB 5 // Dyanmic String Table
#define DT_SYMTAB 6 // Dynamic Symbol Table
#define DT_RELA 7 // Address of relocaiton table
#define DT_RELASZ 8 // Total size (bytes) of the relocation table
#define DT_RELAENT 9 // Total size (bytes) of each relocation entry
#define DT_STRSZ 10 // Total size (bytes) of string table
#define DT_SYMENT 11 // Size (bytes) of symbol table entry
#define DT_INIT 12 // Address of init function
#define DT_FINI 13 // Address of termination function
#define DT_SONAME 14 // String table object of the shared object name
#define DT_RPATH 15 // Table offset of a shared library search path string
#define DT_SYMBOLIC 16

#define DT_REL 17
#define DT_RELSZ 18
#define DT_RELENT 19
#define DT_PLTREL 20
#define DT_DEBUG 21
#define DT_TEXTREL 22
#define DT_JMPREL 23
#define DT_BIND_NOW 24
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6

using ELFHeader = ELF64Header;
using ELFProgramHeader = ELF64ProgramHeader;
using ELFDynamicEntry = ELF64DynamicEntry;

typedef struct process process_t;

int VerifyELF(void* elf);
elf_info_t LoadELFSegments(process_t* proc, void* elf, uintptr_t base);