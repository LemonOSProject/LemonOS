#pragma once

#include <videoconsole.h>

namespace Log{

    void Initialize();
    void SetVideoConsole(VideoConsole* con);

    void Write(char* str, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255);

    void Warning(char* str);
    void Warning(unsigned long long num);

    void Error(char* str);
    void Error(unsigned long long num);

    void Info(char* str);
    void Info(unsigned long long num, bool hex = true);
} 
