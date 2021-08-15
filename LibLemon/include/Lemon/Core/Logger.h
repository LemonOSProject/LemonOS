#pragma once

#include <iostream>
#include <mutex>

namespace Lemon {
namespace Logger {

template<typename Arg>
void Log(const Arg& arg){
    std::cout << arg;
}

const char* GetProgramName();

// Explicit overloads for basic types, no need to pass by const reference
void Log(short arg);
void Log(int arg);
void Log(long arg);
void Log(unsigned short arg);
void Log(unsigned int arg);
void Log(unsigned long arg);
void Log(float arg);

template <typename Arg, typename ...Args>
inline void Log(const Arg& arg, const Args&... args){
    Log(arg);
    Log(args...);
}

template<typename ...Args>
void Debug(const Args&... args){
    Log("[", GetProgramName(), "] [Debug] ", args..., "\n");
}

template<typename ...Args>
void Warning(const Args&... args){
    Log("[", GetProgramName(), "] [Warning] ", args..., "\n");
}

template<typename ...Args>
void Error(const Args&... args){
    Log("[", GetProgramName(), "] [Error] ", args..., "\n");
}

} // namespace Logger
} // namespace Lemon
