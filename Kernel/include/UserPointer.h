#pragma once

#include <Compiler.h>
#include <MM/UserAccess.h>
#include <Paging.h>

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
template <typename T> class UserPointer final {
public:
    UserPointer(uintptr_t ptr) : m_ptr(reinterpret_cast<T*>(ptr)) {}

    [[nodiscard]] ALWAYS_INLINE int GetValue(T& kernelValue) const {
        return user_memcpy(&kernelValue, m_ptr, sizeof(T));
    }
    [[nodiscard]] ALWAYS_INLINE int StoreValue(const T& kernelValue) {
        return user_memcpy(m_ptr, &kernelValue, sizeof(T));
    }

    ALWAYS_INLINE T* Pointer() { return m_ptr; }

    ALWAYS_INLINE operator bool() const { return m_ptr != nullptr; }

private:
    T* m_ptr;
} __attribute__((packed));

template <typename T> class UserBuffer final {
public:
    UserBuffer(uintptr_t ptr) : m_ptr(reinterpret_cast<T*>(ptr)) {}

    [[nodiscard]] ALWAYS_INLINE int GetValue(unsigned index, T& kernelValue) const {
        return user_memcpy(&kernelValue, &m_ptr[index], sizeof(T));
    }
    [[nodiscard]] ALWAYS_INLINE int StoreValue(unsigned index, const T& kernelValue) {
        return user_memcpy(&m_ptr[index], &kernelValue, sizeof(T));
    }

    [[nodiscard]] ALWAYS_INLINE int Read(T* data, size_t offset, size_t count) const {
        if (!IsUsermodePointer(m_ptr, offset, count)) { // Don't allow kernel memory access
            return 1;
        }

        return user_memcpy(data, &m_ptr[offset], sizeof(T) * count);
    }

    [[nodiscard]] ALWAYS_INLINE int Write(T* data, size_t offset, size_t count) {
        if (!IsUsermodePointer(m_ptr, offset, count)) { // Don't allow kernel memory access
            return 1;
        }

        return user_memcpy(&m_ptr[offset], data, sizeof(T) * count);
    }

    ALWAYS_INLINE T* Pointer() { return m_ptr; }

private:
    T* m_ptr;
} __attribute__((packed));

#define TRY_STORE_UMODE_VALUE(ptrObject, value)                                                                        \
    if (ptrObject.StoreValue(value)) {                                                                                 \
        return EFAULT;                                                                                                \
    }

#define TRY_GET_UMODE_VALUE(ptrObject, value)                                                                          \
    if (ptrObject.GetValue(value)) {                                                                                   \
        return EFAULT;                                                                                                \
    }
