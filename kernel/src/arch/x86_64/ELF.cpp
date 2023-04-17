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

Error elf_load_file(const FancyRefPtr<File>& file, ELFData& exe) {
    elf64_header_t header;
    memset(&header, 0, sizeof(elf64_header_t));

    UIOBuffer uio{&header};

    auto bytes = TRY_OR_ERROR(file->Read(0, sizeof(elf64_header_t), &uio));
    if (!VerifyELF(&header)) {
        return ENOEXEC;
    }

    Log::Error("entry at %x", header.entry);

    exe.entry = header.entry;
    exe.phEntrySize = header.phEntrySize;
    exe.phNum = header.phNum;
    exe.exeType = header.type;

    exe.programHeaders.resize(exe.phNum);

    // Initialize all data segment fields to zero
    exe.dataSegments.resize(exe.phNum);
    memset(exe.dataSegments.Data(), 0, exe.phNum * sizeof(uint8_t*));

    for (int i = 0; i < exe.phNum; i++) {
        long offset = header.phOff + i * header.phEntrySize;
        ELF64ProgramHeader* phdr = &exe.programHeaders[i];

        UIOBuffer uio{phdr};

        auto bytes = TRY_OR_ERROR(file->Read(offset, sizeof(ELF64ProgramHeader), &uio));
        if (bytes != sizeof(ELF64ProgramHeader)) {
            Log::Warning("elf_load_file: short read attempting to read program header");
        }

        // Load any program header data
        if (phdr->type == PT_LOAD) {
            uint8_t* data = new uint8_t[phdr->fileSize];
            UIOBuffer uio{data};

            auto bytes = file->Read(phdr->offset, phdr->fileSize, &uio);
            if (bytes.HasError()) {
                Log::Warning("elf_load_file: I/O errors reading elf");

                delete[] data;
                return bytes.Err();
            }

            exe.dataSegments[i] = data;

            if (bytes.Value() != phdr->fileSize) {
                Log::Warning("elf_load_file: short read attempting to load program header data");
            }
        } else if (phdr->type == PT_PHDR) {
            exe.pHdrSegment = phdr->vaddr;
        } else if (phdr->type == PT_INTERP) {
            size_t len = phdr->fileSize;
            if (len > PATH_MAX - 1) {
                len = PATH_MAX - 1;
            }

            UIOBuffer uio{exe.linkerPath};
            TRY_OR_ERROR(file->Read(offset, len, &uio));
            exe.linkerPath[len] = 0;
        } else if (phdr->type == PT_DYNAMIC) {
            uint8_t* buffer = new uint8_t[phdr->fileSize];

            UIOBuffer uio{buffer};

            auto bytes = file->Read(phdr->offset, phdr->fileSize, &uio);
            if (bytes.HasError()) {
                Log::Warning("elf_load_file: I/O errors reading elf");
                delete[] buffer;
                return bytes.Err();
            }

            if (bytes.Value() < phdr->fileSize) {
                Log::Warning("elf_load_file: short read attempting to load dynamic segment");
            }

            ELF64DynamicEntry* dyn = (ELF64DynamicEntry*)buffer;
            // Ensure the whole dynamic entry is within the buffer
            while ((uint8_t*)(dyn + 1) <= (buffer + phdr->fileSize) && dyn->tag != DT_NULL) {
                exe.dynamic.add_back(*dyn);
                dyn++;
            }

            delete[] buffer;
        }
    }

    return ERROR_NONE;
}

Error elf_load_segments(Process* proc, ELFData& exe, uintptr_t base) {
    assert(base == 0 || exe.exeType == ET_DYN);

    exe.entry = base + exe.entry;

    for (unsigned i = 0; i < exe.programHeaders.size(); i++) {
        const auto& header = exe.programHeaders[i];
        if (header.memSize == 0 || header.type != PT_LOAD)
            continue;

        // Do not map zero addresses
        if (base + header.vaddr == 0) {
            Log::Error("elf_load_segments: refusing to map segments to 0x0");
            return EFAULT;
        }

        FancyRefPtr<ProcessImageVMObject> vmo = new ProcessImageVMObject(
            (base + header.vaddr) & ~0xFFFUL,
            ((header.memSize + (header.vaddr & 0xFFF) + 0xFFF) & ~static_cast<uintptr_t>(PAGE_SIZE_4K - 1)), true);
        
        auto* region = proc->addressSpace->MapVMO(static_pointer_cast<VMObject>(vmo), (header.vaddr + base) & ~0xFFFUL, true);
        if (!region) {
            Log::Error("Failed to map process image memory");
            return EFAULT;
        }

        // Don't worry about zeroing,
        // memory is zeroed on allocation
        if (header.fileSize > 0) {
            uint8_t* data = exe.dataSegments[i];
            assert(data);
            // If we aren't the same process then we can't fault in the
            // other process's address space (for now)
            vmo->ForceAllocate();
            vmo->MapAllocatedBlocks(region->Base(), proc->GetPageMap());

            uintptr_t pml4 = Process::Current()->GetPageMap()->pml4Phys;
            asm volatile("cli; mov %%rax, %%cr3" ::"a"(proc->GetPageMap()->pml4Phys) : "memory");
            memcpy((void*)(base + header.vaddr), data, header.fileSize);
            asm volatile("mov %%rax, %%cr3; sti" ::"a"(pml4) : "memory");
        }
    }

    return ERROR_NONE;
}

void elf_free_data(ELFData& data) {
    for(uint8_t* p : data.dataSegments) {
        if(p) {
            delete p;
        }
    }

    data.dynamic.clear();
    data.programHeaders.clear();
    data.dataSegments.clear();
}

extern uint8_t _hiraku[];

HashMap<StringView, HirakuSymbol> symbolMap;
void LoadHirakuSymbols() {
    elf64_header_t* header = (elf64_header_t*)_hiraku;
    elf64_dynamic_t* dynamic = nullptr;

    elf64_program_header_t* phdrs = (elf64_program_header_t*)(_hiraku + header->phOff);
    for (unsigned i = 0; i < header->phNum; i++) {
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

HirakuSymbol* ResolveHirakuSymbol(const char* name) {
    return symbolMap.get(name);
}
