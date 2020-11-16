#pragma once

#include <stdint.h>

template<typename T>
class TTraits{
    static const char* name(){
        return "undefined";
    }
};

template<>
class TTraits <int32_t>{
    static const char* name(){
        return "int32";
    }
};

template<>
class TTraits <uint32_t>{
    static const char* name(){
        return "uint32";
    }
};

template<>
class TTraits <int64_t>{
    static const char* name(){
        return "uint64";
    }
};

template<>
class TTraits <uint64_t>{
    static const char* name(){
        return "uint64";
    }
};