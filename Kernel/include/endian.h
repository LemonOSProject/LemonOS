#pragma once

#include <stdint.h>

static inline uint16_t EndianLittleToBig16(uint16_t val){
    return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

static inline uint32_t EndianLittleToBig32(uint32_t val){
    return (((uint32_t)EndianLittleToBig16(val & 0xFFFF) << 16)) | (EndianLittleToBig16((val >> 16) & 0xFFFF));
}

static inline uint64_t EndianLittleToBig64(uint64_t val){
    return (((uint64_t)EndianLittleToBig32(val & 0xFFFFFFFF)) << 32) | (EndianLittleToBig32((val >> 32) & 0xFFFFFFFF) & 0xFFFFFFFF);
}

static inline uint16_t EndianBigToLittle16(uint16_t val){
    return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

static inline uint32_t EndianBigToLittle32(uint32_t val){
    return (((uint32_t)EndianBigToLittle16(val & 0xFFFF) << 16)) | (EndianBigToLittle16((val >> 16) & 0xFFFF));
}

typedef struct BigEndianUInt16 {
    union{
        uint16_t value;
        struct {
            uint8_t high, low;
        };
    };
    
    BigEndianUInt16& operator=(const uint16_t newValue){
        value = EndianLittleToBig16(newValue);
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
    
    BigEndianUInt16& operator=(const uint16_t newValue){
        value = EndianLittleToBig32(newValue);
    }
    
    operator uint32_t(){
        return EndianBigToLittle32(value);
    }
} __attribute__((packed)) bige_uint32_t;