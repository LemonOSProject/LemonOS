#pragma once

#include <Compiler.h>
#include <Paging.h>

extern "C" {
int UserMemcpy(void* dest, const void* src, size_t count);
void UserMemcpyTrap();
void UserMemcpyTrapHandler();
}

template <typename T, typename P> inline static constexpr int IsUsermodePointer(P* ptr) {
    if (reinterpret_cast<uintptr_t>(ptr + sizeof(T)) >= KERNEL_VIRTUAL_BASE) {
        return 0;
    }

    return 1;
}

template <typename P> inline static constexpr int IsUsermodePointer(P* ptr, size_t offset, size_t count) {
    if (reinterpret_cast<uintptr_t>(ptr + offset + count) >= KERNEL_VIRTUAL_BASE) {
        return 0;
    }

    return 1;
}

// Class for handling usermode pointers
//
template <typename T> class UserPointer {
public:
    UserPointer(uintptr_t ptr) : m_ptr(reinterpret_cast<T*>(ptr)) {}

    [[nodiscard]] ALWAYS_INLINE int GetValue(T& kernelValue) const {
        return UserMemcpy(&kernelValue, m_ptr, sizeof(T));
    }
    [[nodiscard]] ALWAYS_INLINE int StoreValue(const T& kernelValue) {
        return UserMemcpy(m_ptr, &kernelValue, sizeof(T));
    }

    ALWAYS_INLINE T* Pointer() { return m_ptr; }

    ALWAYS_INLINE operator bool() const { return m_ptr != nullptr; }

private:
    T* m_ptr;
};

template <typename T> class UserBuffer {
public:
    UserBuffer(uintptr_t ptr) : m_ptr(reinterpret_cast<T*>(ptr)) {}

    [[nodiscard]] ALWAYS_INLINE int GetValue(unsigned index, T& kernelValue) const {
        return UserMemcpy(&kernelValue, &m_ptr[index], sizeof(T));
    }
    [[nodiscard]] ALWAYS_INLINE int StoreValue(unsigned index, const T& kernelValue) {
        return UserMemcpy(&m_ptr[index], &kernelValue, sizeof(T));
    }

    [[nodiscard]] ALWAYS_INLINE int Read(T* data, size_t offset, size_t count) const {
        if (!IsUsermodePointer(m_ptr, offset, count)) { // Don't allow kernel memory access
            return 1;
        }

        return UserMemcpy(data, &m_ptr[offset], sizeof(T) * count);
    }

    [[nodiscard]] ALWAYS_INLINE int Write(T* data, size_t offset, size_t count) {
        if (!IsUsermodePointer(m_ptr, offset, count)) { // Don't allow kernel memory access
            return 1;
        }

        return UserMemcpy(&m_ptr[offset], data, sizeof(T) * count);
    }

    ALWAYS_INLINE T* Pointer() { return m_ptr; }

private:
    T* m_ptr;
};