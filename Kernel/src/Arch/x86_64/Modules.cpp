#include <Modules.h>

#include <Hash.h>
#include <String.h>
#include <Memory.h>

#include <Fs/Filesystem.h>
#include <ELF.h>

Module::Module(const char* _name) : name(strdup(_name)) {

}

Module::~Module() {

}

namespace ModuleManager {
    HashMap<const char*, Module*> modules;

    int LoadModuleSegments(Module* module, FsNode* file, elf64_header_t header, uintptr_t base) {
        uint16_t phEntrySize = header.phEntrySize;
        uint16_t phNum = header.phNum;
        unsigned phOff = header.phOff;

        elf64_program_header_t programHeader;
        for(uint16_t i = 0; i < phNum; i++){
            if(ssize_t e = fs::Read(file, phOff + i * phEntrySize, sizeof(elf64_header_t), &programHeader); e < sizeof(elf64_header_t)){
                if(e < 0){
                    return e; // Filesystem Error
                } else {
                    return -1; // End of file? Length should be sizeof(elf64_header_t)
                }

                if(programHeader.type == PT_LOAD){
                    for(unsigned j = 0; j < ((programHeader.memSize + (programHeader.vaddr & 0xFFF) + 0xFFF) >> 12); j++){
                        uint64_t phys = Memory::AllocatePhysicalMemoryBlock();
                        Memory::KernelMapVirtualMemory4K(phys, base + (programHeader.vaddr & ~0xFFFUL) + j * PAGE_SIZE_4K, 1);
                    }
                } else if(programHeader.type == PT_DYNAMIC){
                    
                }
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

        modules.insert(module->Name(), module);
        fs::Close(handle);

        return { .status = ModuleLoadStatus::ModuleOK, .code = 0 };
    }

    void UnloadModule() {

    }
}