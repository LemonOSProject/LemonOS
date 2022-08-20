#pragma once

#include <stdint.h>

static inline uint16_t EndianLittleToBig16(uint16_t val){
    return __builtin_bswap16(val);
}

static inline uint32_t EndianLittleToBig32(uint32_t val){
    return __builtin_bswap32(val);
}

static inline uint64_t EndianLittleToBig64(uint64_t val){
    return __builtin_bswap64(val);
}

static inline uint16_t EndianBigToLittle16(uint16_t val){
    return __builtin_bswap16(val);
}

static inline uint32_t EndianBigToLittle32(uint32_t val){
    return __builtin_bswap32(val);
}

typedef struct BigEndianUInt16 {
    union{
        uint16_t value;
        struct {
            uint8_t high, low;
        } __attribute__((packed));
    };

    BigEndianUInt16() {}

    BigEndianUInt16(const uint16_t val) {
        value = EndianLittleToBig16(val);
    }
    
    BigEndianUInt16& operator=(const uint16_t newValue){
        value = EndianLittleToBig16(newValue);
        return *this;
    }

    operator uint16_t(){
        return EndianBigToLittle16(value);
    }
} __attribute__((packed)) bige_uint16_t;

typedef struct BigEndianUInt32 {
    union{
        uint32_t value;
        struct{
            union{
                uint16_t high;
                struct {
                    uint8_t hhigh, hlow;
                };
            };
            union{
                uint16_t low;
                struct {
                    uint8_t lhigh, llow;
                };
            };
        };
    };
    
    BigEndianUInt32& operator=(const uint32_t newValue){
        value = EndianLittleToBig32(newValue);
        return *this;
    }
    
    operator uint32_t(){
        return EndianBigToLittle32(value);
    }
} __attribute__((packed)) bige_uint32_t;