#pragma once

#include <string.h>
#include <stddef.h>

#include <utility>
#include <type_traits>

#include "tuple.h"
#include "string_view.h"

template<typename T>
concept Buffer = requires(T t, char c, const char *s, size_t n) {
    { t.write(c) };
    { t.write(s, n) };
};

template<size_t index, typename ...Types>
struct GetType {
    using type = decltype(Tuple<Types...>{}.template get<index>());
};

template<typename T>
struct TypeFormatter;

struct ConstexprStringTag {};

template<>
struct TypeFormatter<ConstexprStringTag> {
    constexpr TypeFormatter(const char *s, size_t n) : str(s), n(n) {}

    template<Buffer Buffer>
    inline void emit(Buffer &buffer) const {
        buffer.write(str, n);
    }

    const char *str;
    size_t n;
};

template<>
struct TypeFormatter<char> {
    constexpr TypeFormatter() = default;
    
    template<Buffer Buffer>
    inline void emit(char c, Buffer &buffer) const {
        buffer.write(c);
    }
};

template<>
struct TypeFormatter<const char *> {
    constexpr TypeFormatter() = default;

    template<Buffer Buffer>
    inline void emit(const char *s, Buffer &buffer) const {
        buffer.write(s, strlen(s));
    }
};

template<typename T>
constexpr bool format_has_arg() {
    return true;
}

template<>
constexpr bool format_has_arg<TypeFormatter<ConstexprStringTag>>() {
    return false;
}

template<typename F, typename T>
struct FormatStringChunk {
    F formatter;
    T next;

    template<typename Buffer, size_t arg_index, typename ArgTuple>
    constexpr inline void do_format(Buffer &buffer, const ArgTuple &args) const {
        if constexpr (format_has_arg<decltype(formatter)>()) {
            formatter.emit(args.template get<arg_index>(), buffer);

            return next.template do_format<Buffer, arg_index + 1>(buffer, args);
        } else {
            formatter.emit(buffer);

            return next.template do_format<Buffer, arg_index>(buffer, args);
        }
    }
};

template<typename F>
struct FormatStringEnd {
    F formatter;

    template<typename Buffer, size_t arg_index, typename ArgTuple>
    constexpr inline void do_format(Buffer &buffer, const ArgTuple &args) const {
        if constexpr (format_has_arg<decltype(formatter)>()) {
            formatter.emit(args.template get<arg_index>(), buffer);
        } else {
            formatter.emit(buffer);
        }
    }
};

template<typename String, size_t index, size_t arg_index, typename ArgTuple>
consteval auto do_format_string();

template<typename String, size_t index, size_t arg_index, typename ArgTuple>
consteval inline auto do_format_arg() {
    constexpr StringView<typename String::char_type> fmt = String{};

    if constexpr (index < fmt.len()) {
        return FormatStringChunk {
            TypeFormatter<typename std::remove_const<decltype(ArgTuple{}.template get<arg_index>())>::type>{},
            do_format_string<String, index, arg_index + 1, ArgTuple>()
        };
    } else {
        return FormatStringEnd{ TypeFormatter<typename std::remove_const<decltype(ArgTuple{}.template get<arg_index>())>::type>{} };
    }
}

template<typename String, size_t index>
consteval inline size_t index_of_brace_or_end() {
    constexpr StringView<typename String::char_type> fmt = String{};

    if constexpr (index < fmt.len()) {
        if constexpr (fmt[index] == '{') {
            return index;
        } else if constexpr (fmt[index] == '}') {
            return index; 
        } else {
            return index_of_brace_or_end<String, index + 1>();
        }
    } else {
        return index;
    }
}

template<typename String, size_t index, size_t arg_index, typename ArgTuple>
consteval inline auto do_format_string() {
    constexpr StringView<typename String::char_type> fmt = String{};

    constexpr const typename String::char_type *start = &fmt[index];

    constexpr size_t end = index_of_brace_or_end<String, index>();
    if constexpr (fmt[end] == '{') {
        if constexpr (fmt[end + 1] == '}') {
            // We have text inbetween format specifiers
            if constexpr (end - index > 0) {
                return FormatStringChunk {
                    TypeFormatter<ConstexprStringTag>{ start, end - index },
                    do_format_arg<String, end + 2, arg_index, ArgTuple>()
                };
            } else {
                // No text inbetween format specifiers
                return do_format_arg<String, end + 2, arg_index, ArgTuple>();
            }
        } else {
            static_assert(false, "Invalid format: expected closing '}' after '{'");
        }
    } else {
        return FormatStringEnd{ TypeFormatter<ConstexprStringTag>{ start, end - index } };
    }
}

template<typename String, typename ...Args>
consteval auto make_format_string() {
    return do_format_string<String, 0, 0, Tuple<Args...>>();
}

struct FormatString {
    using char_type = char;

    constexpr operator StringView<char>() {
        return "test {}, {}!";
    }
};

template<typename String, typename ...Args>
void format_n_impl(char *buffer, size_t size, Args&&... args) {
    constexpr auto formatter = make_format_string<String, Args...>();

    struct StringBuffer {
        void write(char c) {
            if (index < size) {
                buffer[index++] = c;
            }
        }

        void write(const char *s, size_t n) {
            while (n-- > 0 && index < size) {
                buffer[index++] = *(s++);
            }
        }

        size_t index;
        size_t size;

        char *buffer;
    } b {0, size - 1, buffer};

    formatter.template do_format<StringBuffer, 0, Tuple<Args...>>(b, Tuple<Args...>(std::forward<Args>(args)...));

    buffer[b.size] = 0;
}

#define format_n(buffer, size, str, ...) { \
    struct _FormatString { using char_type = char; constexpr operator StringView<char>() { return str; } };\
    format_n_impl<_FormatString>(buffer, size, __VA_ARGS__); \
}
