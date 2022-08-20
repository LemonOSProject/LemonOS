#include <ELF.h>

#include <CString.h>
#include <Logging.h>
#include <Math.h>
#include <Paging.h>
#include <PhysicalAllocator.h>
#include <Scheduler.h>
#include <StringView.h>
#include <hiraku.h>

int VerifyELF(void* elf) {
    elf64_header_t elfHdr = *(elf64_header_t*)elf;

    char id[4];
    strncpy(id, (char*)elfHdr.id + 1, 3);
    if (strncmp("ELF", id, 3)) {
        Log::Warning("Invalid ELF Header: %x, %x, %x (%c%c%c)", id[0], id[1], id[2], id[0], id[1], id[2]);
        return 0;
    } else
        return 1;
}

elf_info_t LoadELFSegments(Process* proc, void* _elf, uintptr_t base) {
    uint8_t* elf = reinterpret_cast<uint8_t*>(_elf);
    elf_info_t elfInfo;
    memset(&elfInfo, 0, sizeof(elfInfo));

    if (!VerifyELF(elf))
        return elfInfo; // Invalid ELF Header

    elf64_header_t elfHdr = *(elf64_header_t*)elf;
    Log::Error("entry at %x", elfHdr.entry);

    assert(base == 0 || elfHdr.type == ET_DYN);

    elfInfo.entry = base + elfHdr.entry;
    elfInfo.phEntrySize = elfHdr.phEntrySize;
    elfInfo.phNum = elfHdr.phNum;

    for (uint16_t i = 0; i < elfHdr.phNum; i++) {
        elf64_program_header_t elfPHdr = *((elf64_program_header_t*)(elf + elfHdr.phOff + i * elfHdr.phEntrySize));

        if (elfPHdr.memSize == 0 || elfPHdr.type != PT_LOAD)
            continue;

        assert(base + elfPHdr.vaddr);

        proc->usedMemoryBlocks += (elfPHdr.memSize + (elfPHdr.vaddr & 0xFFF) + 0xFFF) >> 12;
        if (!proc->addressSpace->MapVMO(new ProcessImageVMObject((base + elfPHdr.vaddr) & ~0xFFFUL,
                                                                 ((elfPHdr.memSize + (elfPHdr.vaddr & 0xFFF) + 0xFFF) &
                                                                  ~static_cast<uintptr_t>(PAGE_SIZE_4K - 1)),
                                                                 true),
                                        (elfPHdr.vaddr + base) & ~0xFFFUL, true)) {
            Log::Error("Failed to map process image memory");
            memset(&elfInfo, 0, sizeof(elfInfo));
            return elfInfo;
        }
    }

    char* linkPath = nullptr;
    uintptr_t pml4Phys = Scheduler::GetCurrentProcess()->GetPageMap()->pml4Phys;
    for (int i = 0; i < elfHdr.phNum; i++) {
        elf64_program_header_t elfPHdr = *((elf64_program_header_t*)(elf + elfHdr.phOff + i * elfHdr.phEntrySize));

        assert(elfPHdr.fileSize <= elfPHdr.memSize);

        if (elfPHdr.type == PT_LOAD && elfPHdr.memSize > 0) {
            asm volatile("cli; mov %%rax, %%cr3" ::"a"(proc->GetPageMap()->pml4Phys) : "memory");
            memset((void*)(base + elfPHdr.vaddr + elfPHdr.fileSize), 0, (elfPHdr.memSize - elfPHdr.fileSize));
            memcpy((void*)(base + elfPHdr.vaddr), (void*)(elf + elfPHdr.offset), elfPHdr.fileSize);
            asm volatile("mov %%rax, %%cr3; sti" ::"a"(pml4Phys) : "memory");
        } else if (elfPHdr.type == PT_PHDR) {
            elfInfo.pHdrSegment = base + elfPHdr.vaddr;
        } else if (elfPHdr.type == PT_INTERP) {
            linkPath = (char*)kmalloc(elfPHdr.fileSize + 1);
            strncpy(linkPath, (char*)(elf + elfPHdr.offset), elfPHdr.fileSize);
            linkPath[elfPHdr.fileSize] = 0; // Null terminate the path

            elfInfo.linkerPath = linkPath;
        } else if (elfPHdr.type == PT_DYNAMIC) {
            elfInfo.dynamic = (elf64_dynamic_t*)(base + elfPHdr.vaddr);
        }
    }

    return elfInfo;
}

extern uint8_t _hiraku[];

HashMap<StringView, HirakuSymbol> symbolMap;
void LoadHirakuSymbols() {
    elf64_header_t* header = (elf64_header_t*)_hiraku;
    elf64_dynamic_t* dynamic = nullptr;

    elf64_program_header_t* phdrs = (elf64_program_header_t*)(_hiraku + header->phOff);
    for (unsigned i = 0; i < header->phOff; i++) {
        if (phdrs[i].type == PT_DYNAMIC) {
            dynamic = (elf64_dynamic_t*)(_hiraku + phdrs[i].offset);
            break;
        }
    }

    assert(dynamic);

    elf64_symbol_t* symtab = nullptr;
    uint32_t* hashTable = nullptr;

    char* strtab = nullptr;

    while (dynamic->tag != DT_NULL) {
        if (dynamic->tag == DT_STRTAB) {
            strtab = (char*)(_hiraku + dynamic->ptr);
        } else if (dynamic->tag == DT_SYMTAB) {
            symtab = (elf64_symbol_t*)(_hiraku + dynamic->ptr);
        } else if (dynamic->tag == DT_HASH) {
            hashTable = (uint32_t*)(_hiraku + dynamic->ptr);
        }

        dynamic++;
    }

    assert(symtab && strtab && hashTable);

    uint32_t symCount = hashTable[1];
    for (unsigned i = 0; i < symCount; i++) {
        elf64_symbol_t eSym = symtab[i];
        if (ELF64_SYM_BIND(eSym.info) != STB_WEAK || !eSym.name) {
            continue;
        }

        HirakuSymbol sym;
        sym.name = strdup(strtab + eSym.name);
        sym.address = eSym.value;

        symbolMap.insert(sym.name, sym);
    }
}

HirakuSymbol* ResolveHirakuSymbol(const char* name) { return symbolMap.get(name); }
