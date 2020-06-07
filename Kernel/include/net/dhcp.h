#pragma once

#include <stdint.h>
#include <endian.h>
#include <net/ipv4.h>

struct DHCPHeader {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint8_t xID[4];
    BigEndianUInt16 secs;
    BigEndianUInt16 flags;
    IPv4Address clientIP;
    IPv4Address yourIP;
    IPv4Address serverIP;
    IPv4Address gatewayIP;
};