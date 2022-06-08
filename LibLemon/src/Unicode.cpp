#include <Lemon/Core/Unicode.h>

#include <stdio.h>

namespace Lemon {

std::vector<int32_t> UTF8ToUTF32(const std::string& utf8String) {
    std::vector<int32_t> codepoints;
    for(unsigned i = 0; i < utf8String.length(); i++) {
        int c = utf8String[i];
        
        // Check amount of bytes in code point
        if((c & 0xF8) == 0xF0) {
            // 11110xxx
            // 4 bytes
            if(i + 3 >= utf8String.length()) {
                codepoints.push_back(0);
                continue; // Invalid code point
            }

            int codepoint = 0;
            codepoint |= ((c & 0x7U) << 18);

            c = utf8String[++i];
            codepoint |= ((c & 0x3fU) << 12);

            c = utf8String[++i];
            codepoint |= ((c & 0x3fU) << 6);

            c = utf8String[++i];
            codepoint |= (c & 0x3fU);
            
            codepoints.push_back(codepoint);
        } else if((c & 0xF0) == 0xE0) {
            // 1110xxxx
            // 3 bytes

            if(i + 2 >= utf8String.length()) {
                codepoints.push_back(0);
                continue; // Invalid code point
            }

            int codepoint = 0;
            codepoint |= ((c & 0xfU) << 12);

            c = utf8String[++i];
            codepoint |= ((c & 0x3fU) << 6);

            c = utf8String[++i];
            codepoint |= (c & 0x3fU);

            codepoints.push_back(codepoint);
        } else if((c & 0xE0) == 0xC0) {
            // 110xxxxx
            // 2 bytes

            if(i + 1 >= utf8String.length()) {
                codepoints.push_back(0);
                continue; // Invalid code point
            }

            int codepoint = 0;
            codepoint |= ((c & 0x1fU) << 6);

            c = utf8String[++i];
            codepoint |= (c & 0x3fU);

            codepoints.push_back(codepoint);
        } else {
            // 0xxxxxxx
            // 1 byte
            
            codepoints.push_back(c);
        }
    }

    return codepoints;
}

};