#pragma once

#include <string>
#include <vector>

#include <stdint.h>

namespace Lemon {
    
std::vector<int32_t> UTF8ToUTF32(const std::string& utf8String);

};