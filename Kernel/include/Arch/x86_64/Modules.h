#pragma once

#include <Vector.h>
#include <Symbols.h>

#include <ELF.h>

struct ModuleLoadStatus {
    enum {
        ModuleOK = 0,
        ModuleNotFound,
        ModuleInvalid,
        ModuleFailure,
    } status = ModuleFailure;
    int code;
};

enum ModuleFailureCode {
    ErrorUnknown,
    ErrorInvalidELFSection,
    ErrorUnresolvedSymbol,
    ErrorNoModuleInfo,
};

struct ModuleSegment {
    uintptr_t base;
    size_t size;
    union {
        uint32_t attributes;
        struct {
            uint32_t write : 1;
            uint32_t execute : 1;
        };
    };
};

class FsNode;
class Module;

namespace ModuleManager {
    ModuleLoadStatus LoadModule(const char* path);
    int UnloadModule(const char* name);
    void UnloadModule(Module* module);

    int LoadModuleSegments(Module* module, FsNode* file, elf64_header_t& header);
}

class Module {
    friend int ModuleManager::LoadModuleSegments(Module* module, FsNode* file, elf64_header_t& header);
    friend void ModuleManager::UnloadModule(Module* module);

public:
    Module();
    ~Module();

    const char* Name() { return name; }

    int(*init)(void); // Module initialization function
    int(*exit)(void); // Module exit function
protected:
    Vector<ModuleSegment> segments;

    Vector<KernelSymbol*> globalSymbols;
    Vector<KernelSymbol*> localSymbols;

    char* name = nullptr;
    char* description = nullptr;
};