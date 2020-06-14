#pragma once

#include <endian.h>

struct UDPHeader {
    BigEndianUInt16 srcPort;
    BigEndianUInt16 destPort;
    BigEndianUInt16 length;
    BigEndianUInt16 checksum;
};

