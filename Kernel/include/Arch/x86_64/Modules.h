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
    void UnloadModule(const char* name);

    int LoadModuleSegments(Module* module, FsNode* file, elf64_header_t& header);
}

class Module {
    friend int ModuleManager::LoadModuleSegments(Module* module, FsNode* file, elf64_header_t& header);

public:
    Module(const char* name);
    ~Module();

    const char* Name() { return name; }
protected:
    Vector<ModuleSegment> segments;

    Vector<KernelSymbol*> globalSymbols;
    Vector<KernelSymbol*> localSymbols;

    char* name = nullptr;
};