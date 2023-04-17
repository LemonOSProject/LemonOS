#pragma once

#include <Assert.h>
#include <Compiler.h>
#include <Errno.h>
#include <Move.h>

#include <stdint.h>

struct Error {
    int code;

    ALWAYS_INLINE operator int() {
        return code;
    }
} __attribute__((packed));

#define ERROR_NONE (Error{0})

template<typename T, typename E>
struct Result final {
    Result(Result&& other) {
        if(other.m_hasData) {
            m_hasData = true;
            new (data) T(std::move(other.Value()));
            other.m_hasData = false;
        } else if(other.m_hasError) {
            m_hasError = true;
            err = other.err;
        }
    }
    Result(const Result<T, E>&) = delete;

    Result& operator=(Result&& other) {
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
    
    ALWAYS_INLINE ~Result() {
        if(m_hasData) {
            ((T*)data)->~T();
        }
    }

    ALWAYS_INLINE Result(T val)
        : m_hasData(true) {
            new (data) T(std::move(val));
        }

    ALWAYS_INLINE Result(E val)
        : err(val), m_hasError(true) {}

    ALWAYS_INLINE bool HasError() const {
        return m_hasError;
    }

    ALWAYS_INLINE T& Value() {
        assert(!m_hasError);
        return *((T*)data);
    }

    ALWAYS_INLINE E Err() {
        assert(m_hasError);
        return err;
    }

    ALWAYS_INLINE bool operator==(const T& r) {
        return !HasError() && Value() == r;
    }

    ALWAYS_INLINE bool operator!=(const T& r) {
        return HasError() || Value() != r;
    }

    union {
        uint8_t data[sizeof(T)];
        E err;
    };

private:
    bool m_hasError = false;
    bool m_hasData = false;
};

template<typename T>
using ErrorOr = Result<T, Error>;

#define TRY(func) ({auto result = func; if(result != ERROR_NONE) { return result; }})
#define TRY_OR_ERROR(func) ({auto result = func; if (result.HasError()) { return result.err; } std::move(result.Value());})
