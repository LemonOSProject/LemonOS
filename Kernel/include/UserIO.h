#pragma once

#include <Compiler.h>
#include <CString.h>

#include <MM/UserAccess.h>

class UIOBuffer final {
public:
    ALWAYS_INLINE UIOBuffer(void* buffer)
        : m_data((uint8_t*)buffer), m_user(false) {}

    // Create a UIOBuffer from a user pointer
    static UIOBuffer FromUser(void* buffer);

    [[nodiscard]] ALWAYS_INLINE int Read(uint8_t* buffer, size_t len) {
        if(m_user) {
            int r = user_memcpy(buffer, m_data + m_offset, len);
            m_offset += len;
            return r;
        } else {
            memcpy(buffer, m_data + m_offset, len);
            m_offset += len;
            return 0;
        }
    }

    [[nodiscard]] ALWAYS_INLINE int Read(uint8_t* buffer, size_t len, size_t offset) {
        if(m_user) {
            int r = user_memcpy(buffer, m_data + offset, len);
            return r;
        } else {
            memcpy(buffer, m_data + offset, len);
            return 0;
        }
    }

    [[nodiscard]] ALWAYS_INLINE int Write(uint8_t* buffer, size_t len) {
        if(m_user) {
            int r = user_memcpy(m_data + m_offset, buffer, len);
            m_offset += len;
            return r;
        } else {
            memcpy(m_data + m_offset, buffer, len);
            m_offset += len;
            return 0;
        }
    }

    [[nodiscard]] ALWAYS_INLINE int Write(uint8_t* buffer, size_t len, size_t offset) {
        if(m_user) {
            int r = user_memcpy(m_data + offset, buffer, len);
            return r;
        } else {
            memcpy(m_data + offset, buffer, len);
            return 0;
        }
    }

    ALWAYS_INLINE size_t Offset() const { return m_offset; }
    ALWAYS_INLINE void SetOffset(size_t off) { m_offset = off; }

private:
    ALWAYS_INLINE UIOBuffer(void* buffer, size_t offset, bool user)
        : m_data((uint8_t*)buffer), m_offset(offset), m_user(user) {}

    uint8_t* m_data;
    size_t m_offset = 0;

    bool m_user;
};