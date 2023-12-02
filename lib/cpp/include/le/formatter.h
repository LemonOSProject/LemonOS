#pragma once

#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include <type_traits>
#include <limits>
#include <utility>

#include "tuple.h"
#include "string_view.h"

template<typename T>
concept Buffer = requires(T t, char c, const char *s, size_t n) {
    { t.write(c) };
    { t.write(s, n) };
};

template<typename T>
concept Number = requires(T t) {
    { std::is_integral_v<T> == true };
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
    constexpr TypeFormatter(StringView<char> specifier) {
        const_assert(specifier.len() == 0);
    }
    
    template<Buffer Buffer>
    inline void emit(char c, Buffer &buffer) const {
        buffer.write(c);
    }
};

template<>
struct TypeFormatter<const char *> {
    constexpr TypeFormatter(StringView<char> specifier) {
        const_assert(specifier.len() == 0);
    }

    template<Buffer Buffer>
    inline void emit(const char *s, Buffer &buffer) const {
        buffer.write(s, strlen(s));
    }
};

template<Number N>
struct TypeFormatter<N> {
    constexpr TypeFormatter(StringView<char> specifier) {
        if (specifier.len() == 0) {
            return;
        }

        const_assert(specifier[0] == ':');
        for (size_t i = 1; i < specifier.len(); i++) {
            if (specifier[i] == 'x') {
                hex = true;
            }
        }
    }

    template<Buffer Buffer>
    inline void emit(N num, Buffer &buffer) const {
        if (num == 0) {
            buffer.write('0');
            return;
        }

        if constexpr (std::numeric_limits<N>::is_signed) {
            if (num < 0) {
                buffer.write('-');
                num = -num;
            }
        }

        if (hex) {
            constexpr size_t digits_max = std::numeric_limits<N>::digits / 4 + 1;
            char buf[digits_max];

            char * s = buf + digits_max;
            char * const end = s;

            while (num) {
                char digit = (num & 0xf);
                *--s = digit + ((digit > 9) ? 'a' - 10 : '0');
                num /= 16;
            }

            buffer.write(s, end - s);
        } else {
            char buf[std::numeric_limits<N>::digits10 + 1];

            char * s = buf + std::numeric_limits<N>::digits10 + 1;
            char * const end = s;

            while (num) {
                *--s = (num % 10) + '0';
                num /= 10;
            }

            buffer.write(s, end - s);
        }
    }

    bool hex = false;
};

template<typename T>
struct TypeFormatter<T*> : TypeFormatter<uintptr_t> {
    constexpr TypeFormatter(StringView<char> specifier) : TypeFormatter<uintptr_t>(specifier) {
        hex = true;
    }

    template<Buffer Buffer>
    inline void emit(T *ptr, Buffer &buffer) const {
        buffer.write("0x", 2);

        TypeFormatter<uintptr_t>::emit((uintptr_t)ptr, buffer);
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
    constexpr FormatStringChunk(F &&formatter, T &&next) : formatter(formatter), next(next) {}

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
    constexpr FormatStringEnd(F formatter) : formatter(formatter) {}

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
consteval inline auto do_format_arg(StringView<typename String::char_type> specifier) {
    constexpr StringView<typename String::char_type> fmt = String{};

    if constexpr (index < fmt.len()) {
        return FormatStringChunk {
            TypeFormatter<typename std::remove_const<decltype(ArgTuple{}.template get<arg_index>())>::type>(specifier),
            do_format_string<String, index, arg_index + 1, ArgTuple>()
        };
    } else {
        return FormatStringEnd {
            TypeFormatter<typename std::remove_const<decltype(ArgTuple{}.template get<arg_index>())>::type>(specifier)
        };
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

template<typename String, size_t index>
consteval inline StringView<typename String::char_type> format_specifier() {
    constexpr StringView<typename String::char_type> fmt = String{};

    if constexpr (index < fmt.len()) {
        constexpr size_t end = index_of_brace_or_end<String, index>();
        static_assert(fmt[end] == '}', "Invalid format: expected closing '}' after '{'");

        if (end - index > 0) {
            return fmt.substr(index, end - index);
        } else {
            return StringView<typename String::char_type>{};
        }
    } else {
        static_assert(index < fmt.len(), "Invalid format: expected closing '}' after '{'");
    }
}

template<typename String, size_t index, size_t arg_index, typename ArgTuple>
consteval inline auto do_format_string() {
    constexpr StringView<typename String::char_type> fmt = String{};

    constexpr const typename String::char_type *start = &fmt[index];

    constexpr size_t end = index_of_brace_or_end<String, index>();
    if constexpr (fmt[end] == '{') {
        constexpr auto specifier = format_specifier<String, end + 1>();
        constexpr size_t next = (end + 1) + specifier.len() + 1;

        // We have text inbetween format specifiers
        if constexpr (end - index > 0) {
            return FormatStringChunk {
                TypeFormatter<ConstexprStringTag>{ start, end - index },
                do_format_arg<String, next, arg_index, ArgTuple>(specifier)
            };
        } else {
            // No text inbetween format specifiers
            return do_format_arg<String, next, arg_index, ArgTuple>(specifier);
        }
    } else {
        return FormatStringEnd{ TypeFormatter<ConstexprStringTag>{ start, end - index } };
    }
}

template<typename String, typename ...Args>
consteval auto make_format_string() {
    return do_format_string<String, 0, 0, Tuple<Args...>>();
}

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

    buffer[b.index] = 0;
}

template<typename String>
void format_n_impl(char *buffer, size_t size) {
    size_t fmt_len = StringView<typename String::char_type>(String{}).len();
    memcpy(buffer, StringView<typename String::char_type>(String{}).str(), min(size, fmt_len));

    buffer[min(size, fmt_len)] = 0;
}

#define format_n(buffer, size, str, ...) { \
    struct _FormatString { using char_type = char; constexpr operator StringView<char>() { return str; } };\
    format_n_impl<_FormatString>(buffer, size __VA_OPT__(,) __VA_ARGS__); \
}
