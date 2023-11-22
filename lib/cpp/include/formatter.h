#pragma once

#include <stddef.h>

#include <string_view>
#include <type_traits>

#include "tuple.h"

template<typename T>
concept Buffer = requires(T t, char c, const char *s, size_t n) {
    { t.write(c) };
    { t.write(s, n) };
};

template<size_t index, typename ...Types>
struct GetType {
    using type = decltype(Tuple<Types...>{}.get(index));
};

template<typename T>
struct TypeFormatter;

template<>
struct TypeFormatter<char> {
    template<Buffer Buffer>
    static constexpr void emit(char c, Buffer &buffer) {
        buffer.write(c);
    }
};

template<typename char_type, typename ...Args>
constexpr auto make_format_string(const char_type * fmt) {
    
}

template<typename ...Args>
void format_n(char* buffer, size_t size, FormatString<char> format, Args ...args) {
    formatter<Args...> formatter;
}
