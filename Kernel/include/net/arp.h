#pragma once

#include <stdint.h>
#include <net/mac.h>
#include <net/ipv4.h>
#include <endian.h>

#define ETHERTYPE_ARP 0x0806

struct EthernetFrame {
    MACAddress dest; // Destination MAC Address
    MACAddress src; // Source MAC Address
    BigEndianUInt16 etherType;
} __attribute__((packed));

struct ARPHeader {
    BigEndianUInt16 hwType; // Hardware Type
    BigEndianUInt16 prType; // Protocol Type
    uint8_t hLength = 6; // Hardware Address Length
    uint8_t pLength = 4; // Protocol Address Length
    BigEndianUInt16 opcode; // ARP Operation Code
    MACAddress srcHwAddr; // Source hardware address
    IPv4Address srcPrAddr; // Source protocol address
    MACAddress destHwAddr; // Destination hardware address
    IPv4Address destPrAddr; // Destination protocol address
} __attribute__((packed));

struct ARPPacket{
    EthernetFrame frame;
    ARPHeader arp;
} __attribute__((packed));