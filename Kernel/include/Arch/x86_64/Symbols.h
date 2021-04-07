#pragma once

#include <ELF.h>
#include <Fs/Filesystem.h>

struct KernelSymbol {
    uintptr_t address;
    char* mangledName;
};

void LoadSymbolsFromFile(FsNode* node);

int ResolveKernelSymbol(const char* mangledName, KernelSymbol*& symbolPtr);
int ResolveKernelSymbol(uintptr_t address, KernelSymbol*& symbolPtr);

void AddKernelSymbol(KernelSymbol* sym);
void RemoveKernelSymbol(const char* mangledName);