#pragma once

#include <stdint.h>

#include <Compiler.h>

template<typename T>
class TTraits{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "undefined";
    }

    // A type is trivial if the constructor is defaulted (implicitly defined or explicity defaulted)
    // Trivial types can be copied with memcpy, etc.
    // This is important for Vector, etc.
    ALWAYS_INLINE static constexpr bool is_trivial(){
        return false;
    }
};

template<>
class TTraits <int8_t>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "int8";
    }

    ALWAYS_INLINE static constexpr bool is_trivial(){
        return true;
    }
};

template<>
class TTraits <uint8_t>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "uint8";
    }

    ALWAYS_INLINE static constexpr bool is_trivial(){
        return true;
    }
};

template<>
class TTraits <int16_t>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "int16";
    }

    ALWAYS_INLINE static constexpr bool is_trivial(){
        return true;
    }
};

template<>
class TTraits <uint16_t>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "uint16";
    }

    ALWAYS_INLINE static constexpr bool is_trivial(){
        return true;
    }
};

template<>
class TTraits <int32_t>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "int32";
    }

    ALWAYS_INLINE static constexpr bool is_trivial(){
        return true;
    }
};

template<>
class TTraits <uint32_t>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "uint32";
    }

    ALWAYS_INLINE static constexpr bool is_trivial(){
        return true;
    }
};

template<>
class TTraits <int64_t>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "int64";
    }

    ALWAYS_INLINE static constexpr bool is_trivial(){
        return true;
    }
};

template<>
class TTraits <uint64_t>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "uint64";
    }

    ALWAYS_INLINE static constexpr bool is_trivial(){
        return true;
    }
};

template<>
class TTraits <class KernelObject>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "KernelObject";
    }
};

template<>
class TTraits <class Service>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "Service";
    }
};

template<>
class TTraits <class MessageInterface>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "MessageInterface";
    }
};

template<>
class TTraits <class MessageEndpoint>{
public:
    ALWAYS_INLINE static constexpr const char* name(){
        return "MessageEndpoint";
    }
};