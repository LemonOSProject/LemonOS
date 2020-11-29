#pragma once

#include <stdint.h>

template<typename T>
class TTraits{
public:
    static const char* name(){
        return "undefined";
    }
};
template<>
class TTraits <int32_t>{
public:
    static const char* name(){
        return "int32";
    }
};

template<>
class TTraits <uint32_t>{
public:
    static const char* name(){
        return "uint32";
    }
};

template<>
class TTraits <int64_t>{
public:
    static const char* name(){
        return "uint64";
    }
};

template<>
class TTraits <class KernelObject>{
public:
    static const char* name(){
        return "KernelObject";
    }
};

template<>
class TTraits <class Servce>{
public:
    static const char* name(){
        return "Service";
    }
};

template<>
class TTraits <class MessageInterface>{
public:
    static const char* name(){
        return "MessageInterface";
    }
};

template<>
class TTraits <class MessageEndpoint>{
public:
    static const char* name(){
        return "MessageEndpoint";
    }
};

template<>
class TTraits <uint64_t>{
public:
    static const char* name(){
        return "uint64";
    }
};