#pragma once

#include <stdint.h>

template<typename T>
class TTraits{
    static const char* name(){
        return "undefined";
    }
};

template<int32_t>
class TTraits{
    static const char* name(){
        return "int32";
    }
};

template<uint32_t>
class TTraits{
    static const char* name(){
        return "uint32";
    }
};

template<int64_t>
class TTraits{
    static const char* name(){
        return "uint64";
    }
};

template<uint64_t>
class TTraits{
    static const char* name(){
        return "uint64";
    }
};