#pragma once

#include <stddef.h>

#include <tuple>

template<typename ...Types>
struct Tuple {
    template<typename ...Args>
    struct Impl;

    template<typename T, typename ...Args>
    struct Impl<T, Args...> {
        constexpr Impl(T value, Args ...args) : value(value), next(args...) {}
        constexpr Impl() : value(), next() {}
        
        template<size_t index>
        constexpr auto get() const {
            if constexpr (index == 0) {
                return value;
            } else {
                return next.template get<index - 1>();
            }
        }

        T value;
        Impl<Args...> next;
    };

    template<typename T>
    struct Impl<T> {
        constexpr Impl(T value) : value(value) {}
        constexpr Impl() : value() {}

        template<size_t index>
        constexpr auto get() const {
            static_assert(index == 0);
            return value;
        }
        
        T value;
    };

    constexpr Tuple(Types ...values) : impl(values...) {}
    constexpr Tuple() : impl() {}

    template<size_t index>
    constexpr auto get() const {
        return impl.template get<index>();
    }

    Impl<Types...> impl;
};
