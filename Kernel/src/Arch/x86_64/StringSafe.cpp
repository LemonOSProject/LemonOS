#include <CString.h>

#include <Paging.h>

long strlenSafe(const char* str, size_t& size, AddressSpace* aSpace) {
    size_t pageBoundary =
        PAGE_SIZE_4K - (reinterpret_cast<uintptr_t>(str) & (PAGE_SIZE_4K - 1)); // Get amount of bytes to page boundary
    if (!Memory::CheckUsermodePointer(reinterpret_cast<uintptr_t>(str), pageBoundary, aSpace)) {
        return 1;
    }

    size_t& i = size;
    i = 0;

    while (i < pageBoundary) {
        if (str[i] == '\0') {
            return 0;
        }

        i++;
    }

    for (;;) {
        if (!Memory::CheckUsermodePointer(reinterpret_cast<uintptr_t>(str) + i, PAGE_SIZE_4K, aSpace)) {
            return 1;
        }

        size_t pageBoundary = i + PAGE_SIZE_4K;
        while (i < pageBoundary) {
            if (str[i] == '\0') {
                return 0;
            }

            i++;
        }
    }
}
