#include <Modules.h>

#include <Hash.h>
#include <String.h>
#include <Memory.h>

#include <Fs/Filesystem.h>
#include <ELF.h>

Module::Module(const char* _name) : name(strdup(_name)) {

}

Module::~Module() {
    if(name){
        kfree(name);
    }
}

namespace ModuleManager {
    HashMap<const char*, Module*> modules;

    int LoadModuleSegments(Module* module, FsNode* file, elf64_header_t& header) {
        unsigned shOff = header.shOff; // Section header table offset
        uint16_t shEntSize = header.shEntrySize; // Section header table
        uint16_t shNum = header.shNum;
        uint16_t shStrIndex = header.shStrIndex;

        //void* regionStart = nullptr;
        //size_t regionSize = 0;

        ELF64Section* sections = new ELF64Section[shNum];
        ELF64Section* shStrTab = nullptr; // Section String Table
        ELF64Section* symStrTab = nullptr; // Symbol String Table
        ELF64Section* symTab = nullptr; // Symbol table

        for(unsigned i = 0; i < shNum; i++){
            fs::Read(file, shOff + i * shEntSize, sizeof(ELF64Section), &sections[i]);

            if(sections[i].type == SHT_STRTAB){
                if(i == shStrIndex){
                    shStrTab = &sections[i];
                }
            } else if(sections[i].type == SHT_SYMTAB){
                symTab = &sections[i];
            }
        }

        if(!shStrIndex){
            Log::Error("Error loading module: Could not find section string table");
            return -1;
        } else if(!symTab){
            Log::Error("Error loading module: Could not find symbol table");
            return -1;
        }

        char* sectionStringTable = new char[shStrTab->size + 1];
        {
            long len = fs::Read(file, shStrTab->off, shStrTab->size, sectionStringTable);
            sectionStringTable[len] = 0;
        }

        for(unsigned i = 0; i < shNum; i++){
            if(sections[i].type == SHT_STRTAB && strncmp(sectionStringTable + sections[i].name, ".strtab", shStrTab->size - sections[i].name) == 0){ // Don't trust that it is NULL terminated
                symStrTab = &sections[i];
            }
            
            Log::Debug(debugLevelModules, DebugLevelVerbose, "Found section %s (%d): %d %x (off %x)", sectionStringTable + sections[i].name, sections[i].type, sections[i].size, sections[i].addr, sections[i].off);
        }

        if(!symStrTab){
            Log::Error("Error loading module: Could not find symbol string table");
            return -1;
        }

        char* symStringTable = new char[symStrTab->size + 1];
        {
            long len = fs::Read(file, symStrTab->off, symStrTab->size, symStringTable);
            symStringTable[len] = 0;
        }
        
        Vector<ELF64Symbol> symbols;
        {
            ELF64Symbol symbol;
            unsigned off = 0;
            while(off < symTab->size){
                fs::Read(file, symTab->off + off, sizeof(ELF64Symbol), &symbol);

                off += sizeof(ELF64Symbol);

                if(symbol.name > symStrTab->size){
                    continue;
                }

                Log::Debug(debugLevelModules, DebugLevelVerbose, "Found symbol (%x): %s : %x", symbol.info, symStringTable + symbol.name, symbol.value);
            }
        }

        return 0;
    }

    ModuleLoadStatus LoadModule(const char* path) {
        FsNode* node = fs::ResolvePath(path);
        fs_fd_t* handle = nullptr;

        if(!node) { // Check if module exists
            Log::Error("Module '%s' not found!", path);
            return { .status = ModuleLoadStatus::ModuleNotFound, .code = 0 };
        } else if(!(handle = fs::Open(node))){ // Make sure we open a handle so the node stays in memory
            Log::Error("Failed to open handle for module '%s'", path);
            return { .status = ModuleLoadStatus::ModuleNotFound, .code = 0 };
        }

        elf64_header_t header;
        long len = fs::Read(node, 0, sizeof(elf64_header_t), &header); // Attempt to read ELF header

        if(len < sizeof(elf64_header_t) || !VerifyELF(&header)){ // Verify ELF header
            Log::Info("Module '%s' is not in ELF format!", path);
            return { .status = ModuleLoadStatus::ModuleInvalid, .code = 0 };
        }

        Module* module = new Module(fs::BaseName(path));

        LoadModuleSegments(module, node, header);

        modules.insert(module->Name(), module);
        fs::Close(handle);

        return { .status = ModuleLoadStatus::ModuleOK, .code = 0 };
    }

    void UnloadModule() {

    }
}