#include <Module.h>
#include <Modules.h>

#include <CString.h>
#include <Hash.h>
#include <Memory.h>
#include <StringView.h>

#include <ELF.h>
#include <Fs/Filesystem.h>
#include <Panic.h>
#include <Symbols.h>

#include <Errno.h>

Module::Module() {}

Module::~Module() {
    if (name) {
        kfree(name);
    }

    if (description) {
        kfree(description);
    }
}

namespace ModuleManager {
HashMap<StringView, Module*> modules;

int LoadModuleSegments(Module* module, FsNode* file, elf64_header_t& header) {
    unsigned shOff = header.shOff;           // Section header table offset
    uint16_t shEntSize = header.shEntrySize; // Section header table
    uint16_t shNum = header.shNum;
    uint16_t shStrIndex = header.shStrIndex;

    ELF64Section* sections = new ELF64Section[shNum];
    ELF64Section* shStrTab = nullptr;  // Section String Table
    ELF64Section* symStrTab = nullptr; // Symbol String Table
    ELF64Section* symTab = nullptr;    // Symbol table

    for (unsigned i = 0; i < shNum; i++) {
        ssize_t r = fs::Read(file, shOff + i * shEntSize, sizeof(ELF64Section), &sections[i]);
        assert(r == sizeof(ELF64Section));

        if (sections[i].type == SHT_STRTAB) {
            if (i == shStrIndex) {
                shStrTab = &sections[i];
            }
        } else if (sections[i].type == SHT_SYMTAB) {
            symTab = &sections[i];
        }
    }

    if (!shStrIndex) {
        Log::Error("Error loading module: Could not find section string table");

        delete[] sections;
        return ModuleFailureCode::ErrorInvalidELFSection;
    } else if (!symTab) {
        Log::Error("Error loading module: Could not find symbol table");

        delete[] sections;
        return ModuleFailureCode::ErrorInvalidELFSection;
    }

    char* sectionStringTable = new char[shStrTab->size + 1];
    {
        long len = fs::Read(file, shStrTab->off, shStrTab->size, sectionStringTable);
        assert(len == shStrTab->size);
        sectionStringTable[len] = 0;
    }

    for (unsigned i = 0; i < shNum; i++) {
        if (sections[i].type == SHT_STRTAB &&
            strncmp(sectionStringTable + sections[i].name, ".strtab", shStrTab->size - sections[i].name) ==
                0) { // Don't trust that it is NULL terminated
            symStrTab = &sections[i];
        }

        Log::Debug(debugLevelModules, DebugLevelVerbose, "Found section %s (%d): %d %x (off %x)",
                   sectionStringTable + sections[i].name, sections[i].type, sections[i].size, sections[i].addr,
                   sections[i].off);
    }

    if (!symStrTab) {
        Log::Error("Error loading module: Could not find symbol string table");

        delete[] sectionStringTable;
        delete[] sections;
        return ModuleFailureCode::ErrorInvalidELFSection;
    }

    char* symStringTable = new char[symStrTab->size + 1];
    {
        long len = fs::Read(file, symStrTab->off, symStrTab->size, symStringTable);
        assert(len == symStrTab->size);
        symStringTable[len] = 0;
    }

    unsigned symCount = symTab->size / sizeof(ELF64Symbol);

    long moduleInfoIndex = -1;
    ELF64Symbol* symbols = new ELF64Symbol[symCount + 1];
    assert(fs::Read(file, symTab->off, symTab->size, symbols) == symTab->size);
    {
        for (unsigned i = 0; i < symCount; i++) {
            ELF64Symbol& symbol = symbols[i];
            if (symbol.name > symStrTab->size || !symbol.name) {
                continue; // Symbol is without a name
            }

            uint8_t symbolType = ELF64_SYM_TYPE(symbol.info);
            uint8_t symbolBinding = ELF64_SYM_BIND(symbol.info);
            if (symbolType == STT_FILE || symbolType == STT_SECTION) {
                continue; // We don't care about FILE or SECTION symbols
            } else if (symbolType != STT_OBJECT && symbolType != STT_FUNC && symbolType != STT_NOTYPE) {
                Log::Error("[Module] Unknown symbol type: %d. Failed to resolve %s.", symbolType,
                           symStringTable + symbol.name);

                delete[] sectionStringTable;
                delete[] symStringTable;
                delete[] sections;
                delete[] symbols;
                return ModuleFailureCode::ErrorUnresolvedSymbol; // Fail as we failed to resolve the symbol
            }

            if (!strcmp(symStringTable + symbol.name, "_moduleInfo")) {
                moduleInfoIndex = i;
            }

            if (symbolBinding == STB_GLOBAL) {
                if (symbol.shIndex == 0) {
                    KernelSymbol* sym = nullptr;
                    if (!ResolveKernelSymbol(symStringTable + symbol.name, sym)) {
                        Log::Error("[Module] Failed to resolve '%s'!", symStringTable + symbol.name);

                        delete[] sectionStringTable;
                        delete[] symStringTable;
                        delete[] sections;
                        delete[] symbols;
                        return ModuleFailureCode::ErrorUnresolvedSymbol; // Fail as we failed to resolve the symbol
                    } else {
                        Log::Debug(debugLevelModules, DebugLevelVerbose, "Resolved kernel symbol: %s : %x",
                                   sym->mangledName, sym->address);
                    }
                } else {
                    Log::Debug(debugLevelModules, DebugLevelVerbose, "Found symbol (%x): %s : %x (shidx %hd)",
                               symbol.info, symStringTable + symbol.name, symbol.value, symbol.shIndex);
                }
            } else if (symbolBinding == STB_WEAK) {
                if (symbol.shIndex == 0) {
                    KernelSymbol* sym = nullptr;
                    if (!ResolveKernelSymbol(symStringTable + symbol.name, sym)) {
                        Log::Error("[Module] Failed to resolve '%s'!", symStringTable + symbol.name);

                        delete[] sectionStringTable;
                        delete[] symStringTable;
                        delete[] sections;
                        delete[] symbols;
                        return ModuleFailureCode::ErrorUnresolvedSymbol; // Fail as we failed to resolve the symbol
                    } else {
                        Log::Debug(debugLevelModules, DebugLevelVerbose, "Resolved kernel symbol: %s : %x",
                                   sym->mangledName, sym->address);
                    }
                } else {
                    Log::Debug(debugLevelModules, DebugLevelVerbose, "Found symbol (%x): %s : %x (shidx %hd)",
                               symbol.info, symStringTable + symbol.name, symbol.value, symbol.shIndex);
                }
            } else if (symbolBinding == STB_LOCAL) {
                Log::Debug(debugLevelModules, DebugLevelVerbose, "Found symbol (%x): %s : %x (shidx %hd)", symbol.info,
                           symStringTable + symbol.name, symbol.value, symbol.shIndex);
            } else {
                Log::Error("[Module] Unknown symbol binding: %d", symbolBinding);

                delete[] sectionStringTable;
                delete[] symStringTable;
                delete[] sections;
                delete[] symbols;
                return ModuleFailureCode::ErrorUnresolvedSymbol; // Fail as we failed to resolve the symbol
            }
        }
    }

    if (moduleInfoIndex < 0) { // Module info structure not found
        Log::Error("[Module] No module information.");

        delete[] sectionStringTable;
        delete[] symStringTable;
        delete[] sections;
        delete[] symbols;
        return ModuleFailureCode::ErrorNoModuleInfo;
    }

    assert(moduleInfoIndex < symCount && symbols[moduleInfoIndex].size == sizeof(LemonModuleInfo));

    for (int i = 0; i < shNum; i++) {
        ELF64Section& section = sections[i];

        if (section.flags & SHF_ALLOC) {
            assert(section.align <= PAGE_SIZE_4K); // Ensure requried alignment is no larger than the page size

            ModuleSegment moduleSegment;
            moduleSegment.write = !!(section.flags & SHF_WRITE); // Set writeable flag accordingly

            uint64_t pageAttributes = PAGE_PRESENT;
            if (moduleSegment.write) {
                pageAttributes |= PAGE_WRITABLE;
            }

            uintptr_t segmentBase = (uintptr_t)Memory::KernelAllocate4KPages(PAGE_COUNT_4K(section.size));
            for (unsigned i = 0; i < PAGE_COUNT_4K(section.size); i++) {
                Memory::KernelMapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), segmentBase + i * PAGE_SIZE_4K,
                                                 pageAttributes);
            }

            moduleSegment.base = segmentBase;
            moduleSegment.size = section.size;
            section.addr = segmentBase; // Update the segment address for later on

            memset(reinterpret_cast<void*>(segmentBase), 0, section.size);

            if (section.type == SHT_PROGBITS) {
                ssize_t read = fs::Read(file, section.off, section.size, reinterpret_cast<void*>(segmentBase));
                assert(read == section.size);

                Log::Debug(debugLevelModules, DebugLevelVerbose,
                           "[Module] ELF section '%s' loaded! Index: %d Write? %Y", sectionStringTable + section.name,
                           i, moduleSegment.write);
            } else {
                Log::Debug(debugLevelModules, DebugLevelVerbose,
                           "[Module] ELF section '%s' loaded and zeroed! Index: %d Write? %Y",
                           sectionStringTable + section.name, i, moduleSegment.write);
            }

            module->segments.add_back(moduleSegment);
        }
    }

    for (int i = 0; i < shNum; i++) {
        ELF64Section& section = sections[i];

        if (section.type == SHT_RELA) {
            assert(section.info < shNum);

            ELF64RelocationA relocation;
            ELF64Section& relSection = sections[section.info]; // The section index of the section the relocation is
                                                               // applied to is in section header info

            Log::Debug(debugLevelModules, DebugLevelVerbose, "Relocation in '%s'",
                       sectionStringTable + relSection.name);
            if (!(relSection.flags & SHF_ALLOC)) { // We only allocate sections with the alloc flag set, so ensure the
                                                   // section exists in memory
                continue; // debug_info and other sections may have relocations but are not present in memory
            }

            size_t offset = 0;
            while (offset < section.size) {
                ssize_t read = fs::Read(file, section.off + offset, sizeof(ELF64RelocationA), &relocation);
                assert(read == sizeof(ELF64RelocationA));

                offset += read;

                unsigned symIndex = ELF64_R_SYM(relocation.info);
                assert(symIndex < symCount);

                ELF64Symbol& symbol = symbols[symIndex];

                Log::Debug(debugLevelModules, DebugLevelVerbose,
                           "[Module] Found 'rela' relocation. Offset: %x, Addend: %x, Info: %x, Symbol: '%s'",
                           relocation.offset, relocation.addend, relocation.info, symStringTable + symbol.name);

                uintptr_t* relocationPointer = reinterpret_cast<uintptr_t*>(relSection.addr + relocation.offset);

                switch (ELF64_R_TYPE(relocation.info)) {
                case ELF64_R_X86_64_64:
                    if (symbol.shIndex == 0) {
                        KernelSymbol* ksym;
                        int r = ResolveKernelSymbol(symStringTable + symbol.name, ksym);
                        assert(r); // Make sure the symbol is resolved

                        *relocationPointer = ksym->address;
                    } else {
                        assert(symbol.shIndex < shNum);
                        ELF64Section& symSection = sections[symbol.shIndex];
                        *relocationPointer =
                            symSection.addr + symbol.value +
                            relocation.addend; // Symbol value is offset into its section, get the symbol section
                        // Relocation is then address of symbol section + symbol value
                    }
                    break;
                default:
                    Log::Error("[Module] Unknown ELF64 relocation: %d", ELF64_R_TYPE(relocation.info));
                    assert(!"Unsupported module relocation!");
                    break;
                }
            }
        } else {
            assert(section.type != SHT_REL); // We do not currently support rel sections
        }
    }

    for (unsigned i = 0; i < symCount; i++) {
        ELF64Symbol& symbol = symbols[i];
        if (symbol.name > symStrTab->size || !symbol.name) {
            continue; // Symbol is without a name
        }

        uint8_t symbolType = ELF64_SYM_TYPE(symbol.info);
        uint8_t symbolBinding = ELF64_SYM_BIND(symbol.info);
        if (symbolType == STT_FILE || symbolType == STT_SECTION) {
            continue; // We don't care about FILE or SECTION symbols
        }

        assert(symbol.shIndex < shNum);
        ELF64Section& section = sections[symbol.shIndex]; // Get symbol section

        char* symbolName = symStringTable + symbol.name;
        if (symbol.shIndex != 0) {
            if (symbolBinding == STB_GLOBAL) {
                KernelSymbol* sym;
                if (ResolveKernelSymbol(symbolName, sym)) {
                    Log::Error("Symbol '%s' already exists!", symbolName);
                    KernelPanic((const char*[]){"Symbol", symbolName, "Already exists!"}, 3);
                }

                sym = new KernelSymbol(
                    {.address = symbol.value + section.addr, .mangledName = strdup(symbolName)}); // Add symbol to table

                module->globalSymbols.add_back(sym);
                AddKernelSymbol(sym);
            } else if (symbolBinding == STB_WEAK) {
                KernelSymbol* sym;
                if (ResolveKernelSymbol(symbolName, sym)) {
                    Log::Debug(debugLevelModules, DebugLevelNormal,
                               "[Module] Weak symbol '%s' already exists! Will not add to global symbol table.",
                               symbolName);
                }
            } else {
                assert(symbolBinding == STB_LOCAL && symbol.shIndex);
            }
        }
    }

    ELF64Symbol moduleInfoSymbol = symbols[moduleInfoIndex];
    ELF64Section moduleInfoSection = sections[moduleInfoSymbol.shIndex];
    LemonModuleInfo* moduleInfo = reinterpret_cast<LemonModuleInfo*>(moduleInfoSection.addr + moduleInfoSymbol.value);

    module->name = strdup(moduleInfo->name);
    module->description = strdup(moduleInfo->description);
    module->init = moduleInfo->init;
    module->exit = moduleInfo->exit;

    Log::Info("[Module] Module '%s' loaded! Initializtion function at %x, Exit function at %x.", module->Name(),
              module->init, module->exit);

    delete[] sectionStringTable;
    delete[] symStringTable;
    delete[] sections;
    delete[] symbols;
    return 0;
}

ModuleLoadStatus LoadModule(const char* path) {
    FsNode* node = fs::ResolvePath(path);
    if (!node) { // Check if module exists
        Log::Error("Module '%s' not found!", path);
        return {.status = ModuleLoadStatus::ModuleNotFound, .code = 0};
    }

    ErrorOr<UNIXOpenFile*> handle = nullptr;
    if (!(handle = fs::Open(node))) { // Make sure we open a handle so the node stays in memory
        Log::Error("Failed to open handle for module '%s'", path);
        return {.status = ModuleLoadStatus::ModuleNotFound, .code = 0};
    }

    elf64_header_t header;
    long len = fs::Read(node, 0, sizeof(elf64_header_t), &header); // Attempt to read ELF header

    if (len < sizeof(elf64_header_t) || !VerifyELF(&header)) { // Verify ELF header
        Log::Info("Module '%s' is not in ELF format!", path);
        return {.status = ModuleLoadStatus::ModuleInvalid, .code = 0};
    }

    Module* module = new Module();

    if (int err = LoadModuleSegments(module, node, header); err) {
        return {.status = ModuleLoadStatus::ModuleFailure, .code = err};
    }

    modules.insert(module->Name(), module);
    delete handle.Value();

    int status = module->init();
    if (status) {
        Log::Warning("[Module] Module '%s' returned a status code of %d. Unloading module.", module->Name(), status);
        UnloadModule(module);
    }

    return {.status = ModuleLoadStatus::ModuleOK, .code = 0};
}

int UnloadModule(const char* name) {
    Module* module;
    int found = modules.get(name, module);

    if (!found) {
        return -ENOENT;
    }

    UnloadModule(module);

    return 0;
}

void UnloadModule(Module* module) {
    modules.remove(module->name);

    int status = module->exit();
    if (status) {
        // Do not trust kernel state if a module fails to exit cleanly
        KernelPanic((const char*[]){"Failed to unload kernel module:", module->Name()}, 2);
    }

    for (auto& sym : module->globalSymbols) {
        RemoveKernelSymbol(sym->mangledName);
        delete sym;
    }
    module->globalSymbols.clear();

    for (auto& seg : module->segments) {
        for (size_t i = 0; i < PAGE_COUNT_4K(seg.size); i++) {
            uint64_t phys = Memory::VirtualToPhysicalAddress(seg.base + i * PAGE_SIZE_4K);
            Memory::FreePhysicalMemoryBlock(phys);
        }

        Memory::KernelFree4KPages((void*)seg.base, PAGE_COUNT_4K(seg.size));
    }

    delete module;
}
} // namespace ModuleManager