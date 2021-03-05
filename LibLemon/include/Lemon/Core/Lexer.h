#pragma once

#include <string_view>
#include <vector>
#include <cctype>

namespace Lemon{
    // Provides a simple base for a more complex lexer
    class BasicLexer {
    protected:
        int line = 1;
        std::string_view sv;
        const char* it;
    public:
        BasicLexer() = default;
        BasicLexer(const std::string_view& v);

        inline void Restart(){ it = sv.begin(); }
        inline bool End(){ return it >= sv.end(); }

        char Eat();

        bool EatWord(const char* word);

        template<typename C>
        std::string_view EatWhile(C cond){
            if(End()) return nullptr;

            auto start = it;
            size_t count = 0;
            char c;
            while(!End() && cond(c = Peek())){
                count++;

                if(c == '\n'){
                    line++;
                }
                Eat();
            }

            return sv.substr(static_cast<size_t>(start - sv.begin()), count);
        }

        int EatOne(char c);

        inline void EatWhitespace(bool includeBreaks = true) { 
            if(includeBreaks){
                EatWhile([](char c) -> bool { return isspace(c) || isblank(c) || c == '\t' || c == '\n' || c == '\r'; });
            } else {
                EatWhile([](char c) -> bool { return isspace(c) || isblank(c) || c == '\t'; });
            }
        }

        char Peek(long ahead = 0) const;
    };
}