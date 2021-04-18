#pragma once

#include <Video/VideoConsole.h>
#include <stdarg.h>

#include <Debug.h>

namespace Log{
    extern VideoConsole* console;

    void LateInitialize();
    void SetVideoConsole(VideoConsole* con);

    void DisableBuffer();
    void EnableBuffer();

    void WriteF(const char* __restrict format, va_list args);

    void Write(const char* str, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255);
    void Write(unsigned long long num, bool hex = true, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255);
    
    void Print(const char* __restrict fmt, ...);

    //void Warning(const char* str);
    void Warning(unsigned long long num);
    void Warning(const char* __restrict fmt, ...);

    //void Error(const char* str);
    void Error(unsigned long long num, bool hex = true);
    void Error(const char* __restrict fmt, ...);

    //void Info(const char* str);
    void Info(unsigned long long num, bool hex = true);
    void Info(const char* __restrict fmt, ...);

    #ifdef KERNEL_DEBUG
    __attribute__((always_inline)) inline static void Debug(const int& var, const int lvl, const char* __restrict fmt, ...){
        if(var >= lvl){
		    Write("\r\n[INFO]    ");
            va_list args;
            va_start(args, fmt);
            WriteF(fmt, args);
            va_end(args);
        }
    }
    #else
    __attribute__ ((unused, always_inline)) inline static void Debug(...){}
    #endif
} 
