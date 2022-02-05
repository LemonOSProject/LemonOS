#pragma once

#include <Assert.h>
#include <Compiler.h>
#include <Errno.h>
#include <Move.h>

#include <stdint.h>

struct Error {
    int code;
};

template<typename T>
struct ErrorOr final {
    ErrorOr(ErrorOr&& other) {
        if(other.m_hasData) {
            m_hasData = true;
            new (data) T(std::move(other.Value()));
            other.m_hasData = false;
        } else if(other.m_hasError) {
            m_hasError = true;
            err = other.err;
        }
    }
    ErrorOr(const ErrorOr<T>&) = delete;

    ErrorOr& operator=(ErrorOr&& other) {
        if(m_hasData) {
            ((T*)data)->~T();
            m_hasData = false;
        }

        m_hasError = false;

        if(other.m_hasData) {
            m_hasData = true;
            new (data) T(std::move(other.Value()));
            other.m_hasData = false;
        } else if(other.m_hasError) {
            m_hasError = true;
            err = other.err;
        }

        return *this;
    }
    
    ALWAYS_INLINE ~ErrorOr() {
        if(m_hasData) {
            ((T*)data)->~T();
        }
    }

    ALWAYS_INLINE ErrorOr(T val)
        : m_hasData(true) {
            new (data) T(std::move(val));
        }

    ALWAYS_INLINE ErrorOr(Error val)
        : err(val), m_hasError(true) {}

    ALWAYS_INLINE bool HasError() const {
        return m_hasError;
    }

    ALWAYS_INLINE T& Value() {
        assert(!m_hasError);
        return *((T*)data);
    }

    ALWAYS_INLINE Error Err() {
        assert(m_hasError);
        return err;
    }

    union {
        uint8_t data[sizeof(T)];
        Error err;
    };

    ALWAYS_INLINE operator bool() {
        return !m_hasError;
    }

private:
    bool m_hasError = false;
    bool m_hasData = false;
};

#define TRY_OR_ERROR(func) ({auto result = func; if (!result) { return result.err; } std::move(result.Value());})
