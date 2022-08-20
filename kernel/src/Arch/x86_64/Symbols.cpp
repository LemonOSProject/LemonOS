#include <Symbols.h>

#include <Debug.h>

#include <CString.h>
#include <Hash.h>
#include <StringView.h>
#include <Vector.h>
#include <Panic.h>

#include <Logging.h>

HashMap<StringView, KernelSymbol*> symbolHashMap;

extern KernelSymbol _kernel_symbols_start[];
extern char _kernel_symbol_strings[];

void LoadSymbols() {
    unsigned symCount =
        ((uintptr_t)(_kernel_symbol_strings) - (uintptr_t)(_kernel_symbols_start)) / sizeof(KernelSymbol);
    for (unsigned i = 0; i < symCount; i++) {
        AddKernelSymbol(&_kernel_symbols_start[i]);
    }
}

int ResolveKernelSymbol(const char* mangledName, KernelSymbol*& symbolPtr) {
    return symbolHashMap.get(mangledName, symbolPtr);
}

void AddKernelSymbol(KernelSymbol* sym) {
    if(symbolHashMap.find(sym->mangledName)) {
        KernelPanic("Symbol already present: ", sym->mangledName);
    }

    symbolHashMap.insert(sym->mangledName, sym);
}

void RemoveKernelSymbol(const char* mangledName) { symbolHashMap.remove(mangledName); }