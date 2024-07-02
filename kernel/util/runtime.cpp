#include <mm/kmalloc.h>

#include <stddef.h>

void *operator new(size_t sz) {
    return mm::kmalloc(sz);
}

void operator delete(void *ptr) {
    mm::kfree(ptr);
}
