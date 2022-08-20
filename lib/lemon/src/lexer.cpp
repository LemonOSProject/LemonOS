#include <lemon/core/Lexer.h>

#include <cassert>
#include <cstring>

namespace Lemon {
BasicLexer::BasicLexer(const std::string_view& v) {
    sv = v;
    it = sv.begin();
}

char BasicLexer::Eat() {
    assert(!End());
    char c = *it++;

    if (c == '\n') {
        line++;
    }

    return c;
}

bool BasicLexer::EatWord(const char* word) {
    size_t i = 0;
    while (it < sv.end() && i < strlen(word)) {
        char c = *it++;

        if (c == '\n') {
            line++;
        }

        if (c != word[i]) {
            return false;
        }

        i++;
    }

    return true;
}

int BasicLexer::EatOne(char c) {
    assert(!End());

    char ch = *it++;
    if (ch == '\n') {
        line++;
    }

    return ch == c;
}

char BasicLexer::Peek(long ahead) const {
    assert(it + ahead < sv.end());

    return *(it + ahead);
}
} // namespace Lemon
