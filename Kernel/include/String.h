#pragma once

#include <Assert.h>
#include <Compiler.h>
#include <Move.h>
#include <Paging.h>
#include <Spinlock.h>
#include <TTraits.h>
#include <stddef.h>

/////////////////////////////
/// \brief safe strlen
///
/// Get the length of a usermode string and check if the pointer is valid
///
/// \return 0 on success, 1 when invalid pointer / not null terminated
/////////////////////////////
long strlenSafe(const char* str, size_t& size, AddressSpace* aSpace);

char* itoa(unsigned long long num, char* str, int base);

int HexStringToPointer(const char* buffer, size_t bufferSize, uintptr_t& pointerValue);

extern "C" void* memset(void* src, int c, size_t count);
extern "C" void* memcpy(void* dest, const void* src, size_t count);
extern "C" int memcmp(const void* s1, const void* s2, size_t n);

void memcpy_optimized(void* dest, void* src, size_t count);

void strcpy(char* dest, const char* src);
void strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);

char* strtok_r(char* str, const char* delim, char** saveptr);

size_t strlen(const char* str);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);

int strncmp(const char* s1, const char* s2, size_t n);

char* strupr(char* s);
char* strnupr(char* s, size_t n);

char* strchr(const char* s, int c);
char* strnchr(const char* s, int c, size_t n);
char* strrchr(const char* s, int c);

char* strdup(const char* s);

template <typename T> class BasicString {
private:
    // Round buffer to 32 bytes
    const unsigned c_bufferRound = 32;
    static_assert(TTraits<T>::is_trivial());

public:
    BasicString() {
        m_buffer = nullptr;
        m_bufferSize = 0;
        m_len = 0;
        assert(Empty());
    }

    BasicString(const T* data) {
        m_buffer = nullptr;
        CopyFromNullTerminatedBuffer(data);
    }

    BasicString(const BasicString& other) {
        Resize(other.m_len);

        memcpy(m_buffer, other.m_buffer, m_len * sizeof(T));
        m_buffer[m_len] = 0;
    }

    BasicString(BasicString&& other) {
        ScopedSpinLock acq(other.m_lock);

        m_len = other.m_len;
        m_bufferSize = other.m_bufferSize;
        m_buffer = other.m_buffer;

        other.m_len = 0;
        other.m_buffer = 0;
        other.m_bufferSize = 0;
    }

    ~BasicString() {
        ScopedSpinLock acq(m_lock);

        if (m_buffer) {
            delete[] m_buffer;
            m_buffer = nullptr;
        }

        m_len = 0;
        m_bufferSize = 0;
    }

    BasicString& operator=(const BasicString& other) {
        ScopedSpinLock acq(m_lock);
        Resize(other.m_len);

        memcpy(m_buffer, other.m_buffer, m_len * sizeof(T));
        m_buffer[m_len] = 0;

        return *this;
    }

    BasicString& operator=(BasicString&& other) {
        ScopedSpinLock acq(m_lock);
        ScopedSpinLock acqOther(other.m_lock);

        m_len = other.m_len;
        m_bufferSize = other.m_bufferSize;
        m_buffer = other.m_buffer;

        other.m_len = 0;
        other.m_buffer = 0;

        return *this;
    }

    BasicString& operator+=(const BasicString& other) {
        ScopedSpinLock acq(m_lock);

        unsigned oldLen = m_len;
        Resize(oldLen + other.m_len);

        memcpy((m_buffer + oldLen), other.m_buffer, other.m_len * sizeof(T));

        return *this;
    }

    BasicString& operator+=(const T* data) {
        ScopedSpinLock acq(m_lock);

        unsigned oldLen = m_len;
        unsigned dataLen = 0;
        while (data[dataLen])
            dataLen++;

        Resize(oldLen + dataLen);

        memcpy((m_buffer + oldLen), data, dataLen);

        return *this;
    }

    int Compare(const T* data) const {
        const T* s1 = m_buffer;

        while (*s1 == *data) {
            if (!(*s1)) {
                return 0; // Null terminator
            }

            s1++;
            data++;
        }

        return (*s1) - (*data);
    };

    ALWAYS_INLINE int Compare(const BasicString& other) const {
        return Compare(other.Data());
    };

    /////////////////////////////
    /// \brief Get length of string
    /////////////////////////////
    ALWAYS_INLINE unsigned Length() const { return m_len; }

    /////////////////////////////
    /// \brief Get NULL-terminated data of string
    /////////////////////////////
    ALWAYS_INLINE const T* Data() const { return m_buffer; }
    ALWAYS_INLINE const T* c_str() const { return m_buffer; }

    /////////////////////////////
    /// \brief Get whether the string is empty
    /////////////////////////////
    ALWAYS_INLINE bool Empty() const { return (!m_buffer) || (m_len == 0); }

protected:
    lock_t m_lock = 0;

    unsigned m_len = 0;
    unsigned m_bufferSize = 0;
    T* m_buffer = nullptr;

    ALWAYS_INLINE void CopyFromNullTerminatedBuffer(const T* data) {
        unsigned len = 0;
        while (data[len])
            len++; // Get the length of data assuming its null-terminated

        Resize(len);

        memcpy(m_buffer, data, m_len * sizeof(T));
        m_buffer[m_len] = 0; // NULL-terminate
    }

    ALWAYS_INLINE void Resize(unsigned newLen) {
        if (m_buffer && newLen < m_bufferSize) {
            m_len = newLen;
            m_buffer[m_len] = 0; // null-terminate
            return;              // Reallocation not necessary
        }

        // We add one to the m_len so we can null terminate the buffer
        m_bufferSize = (newLen + 1 + c_bufferRound - 1) & ~(c_bufferRound - 1);
        T* newBuffer = new T[m_bufferSize];

        if (m_buffer) {
            memcpy(newBuffer, m_buffer, m_len * sizeof(T));
            delete[] m_buffer;
        }

        m_buffer = newBuffer;
        m_len = newLen;

        m_buffer[m_len] = 0; // null-terminate
    }
};

template <typename T> BasicString<T> operator+(const BasicString<T>& l, const T* r) {
    BasicString<T> str(l);
    str += r;

    return str;
}

template <typename T, typename U> BasicString<T> operator+(const BasicString<T>& l, const U& r) {
    BasicString<T> str(l);
    str += r;

    return str;
}

using String = BasicString<char>;

ALWAYS_INLINE String to_string(int num, int base = 10){
    char buf[128];
    buf[127] = 0;

    int i = 127;
    if (num == 0) {
        buf[--i] = '0';
    }

    while (num != 0 && i) {
        int rem = num % base;
        buf[--i] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    return String(buf + i);
}

ALWAYS_INLINE String to_string(long num, int base = 10){
    char buf[128];
    buf[127] = 0;

    int i = 127;
    if (num == 0) {
        buf[--i] = '0';
    }

    while (num != 0 && i) {
        int rem = num % base;
        buf[--i] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    return String(buf + i);
}

ALWAYS_INLINE String to_string(unsigned num, int base = 10){
    char buf[128];
    buf[127] = 0;

    int i = 127;
    if (num == 0) {
        buf[--i] = '0';
    }

    while (num != 0 && i) {
        int rem = num % base;
        buf[--i] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    return String(buf + i);
}
