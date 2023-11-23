#pragma once

#include <assert.h>
#include <stddef.h>

#include "util.h"

template<typename char_type, typename size_type = size_t>
class StringView {
public:
    using const_pointer = const char_type *;
    using const_reference = const char_type &;

    constexpr StringView(const_pointer str) : m_str{str}, m_len{details_strlen(str)} {}
    constexpr StringView(const_pointer str, size_type len) : m_str{str}, m_len{len} {}

    constexpr const_reference operator[](size_type i) const {
        return m_str[i];
    }

    constexpr size_type len() const {
        return m_len;
    }

    constexpr StringView substr(size_type start, size_type len = (size_t)-1) const {
        return StringView{m_str + start, min(len, m_len - start)};
    }

private:
    constexpr static size_type details_strlen(const_pointer str) {
        size_type n = 0;
        while (str && str[n]) {
            n++;
        }

        return n;
    }

    const char_type *m_str;
    size_type m_len;
};
