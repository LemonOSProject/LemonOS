#pragma once

#include <utility>

template<typename E, typename T>
class Result {
public:
    Result(E&& e)
        : m_err{std::move(e)}, m_is_err{true} {}

    Result(T&& data)
        : m_data{std::move(data)}, m_is_err{false} {}

    ~Result() {
        if (m_is_err) {
            delete (m_err) E();
        } else {
            delete (m_data) T();
        }
    }

    inline bool is_err() const {
        return m_is_err;
    }

    inline E &&move_err() {
        return std::move(m_err);
    }

    inline T &&move_val() {
        return std::move(m_data);
    }

private:
    union {
        E m_err;
        T m_data;
    };

    bool m_is_err;
};

#define TRY(x) { auto v = x; if (v.is_err()) return v.move_err(); v.move_data() }
