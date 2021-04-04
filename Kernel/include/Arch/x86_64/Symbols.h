#pragma once

#include <ELF.h>
#include <Fs/Filesystem.h>

struct KernelSymbol {
    uintptr_t address;
    char* mangledName;
};

void LoadSymbolsFromFile(FsNode* node);

int ResolveSymbol(char* const mangledName, KernelSymbol*& symbolPtr);
int ResolveSymbol(uintptr_t address, KernelSymbol*& symbolPtr);