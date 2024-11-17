#pragma once

#include <le/callback.h>

#include <assert.h>
#include <utility>

// Allows for lambda storage and type-erasure
template<typename ...Args>
class Fn {
public:
    Fn() = default;

    template<typename FnType>
    Fn(FnType l) {
        auto *data = new FnType(std::move(l));
        assert(data);

        m_data = data;
        m_fn = [](void *data, Args... args) {
            auto *fn = (FnType *)data;
            return (*fn)(args...);
        };
        m_delete = [](void *data) {
            auto *fn = (FnType *)data;
            delete fn;
        };
    }

    ~Fn() {
        if (m_data) {
            m_delete(m_data);
        }
    }

    Fn(Fn &&other) noexcept {
        m_data = other.m_data;
        m_fn = other.m_fn;

        other.m_data = nullptr;
        other.m_fn = nullptr;
    }

    Fn &operator=(Fn &&other) noexcept {
        if (this != &other) {
            m_data = other.m_data;
            m_fn = other.m_fn;

            other.m_data = nullptr;
            other.m_fn = nullptr;
        }

        return *this;
    }

    void call(Args... args) {
        m_fn(m_data, args...);
    }

    Callback<Args...> callback() {
        return {
            .data = m_data,
            .fn = m_fn
        };
    }

private:
    void *m_data = nullptr;

    void (*m_fn)(void *, Args...);
    void (*m_delete)(void *);
};