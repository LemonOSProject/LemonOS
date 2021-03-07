#pragma once

#include <Vector.h>

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

class Module {
public:
    Module(const char* name);
    ~Module();

    const char* Name() { return name; }
private:
    struct ModuleSegment {
        ELFProgramHeader elfProgramHeader;
        union {
            struct {
                uintptr_t base;
                size_t len;
            } load;
            struct {
                ELFDynamicEntry* dynamicTable;
            } dynamic;
        };
    };

    const char* name = nullptr;
};

namespace ModuleManager {
    ModuleLoadStatus LoadModule(const char* path);
    void UnloadModule(const char* name);
}