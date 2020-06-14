#pragma once

#include <stdint.h>
#include <string.h>

struct IPv4Address{
    uint8_t data[4];

    IPv4Address& operator=(const uint8_t newData[]){
        memcpy(data, newData, 4);
    }
};