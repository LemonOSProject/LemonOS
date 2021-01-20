#pragma once

#include <stdint.h>

static __attribute__((always_inline)) inline uint16_t EndianLittleToBig16(uint16_t val){
    return __builtin_bswap16(val);
}

static __attribute__((always_inline)) inline uint32_t EndianLittleToBig32(uint32_t val){
    return __builtin_bswap32(val);
}

static __attribute__((always_inline)) inline uint64_t EndianLittleToBig64(uint64_t val){
    return __builtin_bswap64(val);
}

static __attribute__((always_inline)) inline uint16_t EndianBigToLittle16(uint16_t val){
    return __builtin_bswap16(val);
}

static __attribute__((always_inline)) inline uint32_t EndianBigToLittle32(uint32_t val){
    return __builtin_bswap32(val);
}

static __attribute__((always_inline)) inline uint32_t EndianBigToLittle64(uint64_t val){
    return __builtin_bswap64(val);
}

template<typename T>
static __attribute__((always_inline)) inline constexpr T EndianLittleToBig(const T& value){
        if constexpr(sizeof(T) == sizeof(uint64_t)){
            return EndianLittleToBig64(value);
        } else if constexpr(sizeof(T) == sizeof(uint32_t)){
            return EndianLittleToBig32(value);
        } else if constexpr(sizeof(T) == sizeof(uint16_t)){
            return EndianLittleToBig16(value);
        } else {
            static_assert(sizeof(T) == 1);
            return value;
        }
}

template<typename T>
static __attribute__((always_inline)) inline constexpr T EndianBigToLittle(const T& value){
        if constexpr(sizeof(T) == sizeof(uint64_t)){
            return EndianBigToLittle64(value);
        } else if constexpr(sizeof(T) == sizeof(uint32_t)){
            return EndianBigToLittle32(value);
        } else if constexpr(sizeof(T) == sizeof(uint16_t)){
            return EndianBigToLittle16(value);
        } else {
            static_assert(sizeof(T) == 1);
            return value;
        }
}

template<typename T>
struct BigEndian {
    T value;

    BigEndian() : value(0) {};
    BigEndian(const T& newValue) : value(newValue) {}
    
    BigEndian<T>& operator=(const T& newValue){
        value = EndianLittleToBig<T>(newValue);
        return *this;
    }
    
    inline constexpr operator T() const{
        return EndianBigToLittle<T>(value);
    }
} __attribute__((packed));