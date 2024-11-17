#include <mm/kmalloc.h>

#include <stddef.h>

void *operator new(size_t sz) {
    return mm::kmalloc(sz);
}

void *operator new[](size_t sz) {
    return mm::kmalloc(sz);
}

void operator delete(void *ptr) {
    mm::kfree(ptr);
}

void operator delete[](void *ptr) {
    mm::kfree(ptr);
}

extern "C" void __cxa_pure_virtual() {
    __builtin_unreachable();
}
