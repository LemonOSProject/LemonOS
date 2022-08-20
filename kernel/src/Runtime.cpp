#include <Memory.h>
#include <Panic.h>
#include <Lock.h>
#include <stddef.h>

void* operator new(size_t size) { return kmalloc(size); }

void* operator new[](size_t size) { return kmalloc(size); }

void operator delete(void* p) { kfree(p); }

void operator delete(void* p, size_t) { ::operator delete(p); }

void operator delete[](void* p) { kfree(p); }

void operator delete[](void* p, size_t) { ::operator delete[](p); }

extern "C" void __cxa_pure_virtual() {
    const char* reasons[1] = {"__cxa_pure_virtual"};
    KernelPanic(reasons, 1);
}
