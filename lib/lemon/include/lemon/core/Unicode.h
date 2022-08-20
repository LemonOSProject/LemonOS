#pragma once

#include <string>
#include <vector>

#include <stdint.h>

namespace Lemon {
    
std::vector<int32_t> UTF8ToUTF32(const std::string& utf8String);
unsigned UTF8Strlen(const std::string& utf8String);

// Get the position of bytes n codepoints in
// if n < 0, go backwards
unsigned UTF8SkipCodepoints(const std::string& utf8String, long n);

};