#pragma once

#include <stdint.h>
#include <string.h>

typedef struct MACAddress {
    uint8_t data[6];

    MACAddress& operator=(const uint8_t newData[]){
        memcpy(data, newData, 6);
    }
} __attribute__((packed)) mac_address_t;