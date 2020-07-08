#pragma once

#include <stdint.h>
#include <stddef.h>
#include <endian.h>
#include <string.h>

class Socket;

#define PORT_MAX UINT16_MAX

#define EPHEMERAL_PORT_RANGE_START 49152
#define EPHEMERAL_PORT_RANGE_END PORT_MAX

struct NetworkPacket{
    void* data;
    size_t length;
};

struct IPv4Address{
    uint8_t data[4];

    IPv4Address() {}

    IPv4Address(uint8_t one, uint8_t two, uint8_t three, uint8_t four){
        data[0] = one;
        data[1] = two;
        data[2] = three;
        data[3] = four;
    }

    IPv4Address(const uint32_t address){
        *((uint32_t*)data) = address;
    }

    IPv4Address& operator=(const uint8_t newData[]){
        memcpy(data, newData, 4);
    }

    IPv4Address& operator=(const uint32_t address){
        *((uint32_t*)data) = address;
    }
} __attribute__((packed));

struct IPv4Header{ // Keep in mind that our architecture is little endian so the bitfields are swapped
    uint8_t ihl : 4; // Internet Header Length
    uint8_t version : 4; // Should be 4
    uint8_t ecn : 2; // Explicit Congestion Notification
    uint8_t dscp : 6; // Differentiated Services Code Point
    BigEndianUInt16 length; // Total Length - Packet size in bytes including header
    BigEndianUInt16 id; // ID
    uint16_t fragmentOffset : 13; // Offset of the fragment
    uint16_t flags : 3; // 0 - Reserved, 1 - Don't Fragment, 2 - More Fragments
    uint8_t ttl; // Time to live
    uint8_t protocol; // Protocol
    BigEndianUInt16 headerChecksum; // Checksum
    IPv4Address sourceIP; // Source IP Address
    IPv4Address destIP; // Destination IP Address
    uint8_t data[];
} __attribute__((packed));

typedef struct MACAddress {
    uint8_t data[6];

    MACAddress& operator=(const uint8_t newData[]){
        memcpy(data, newData, 6);
    }

    bool operator==(const MACAddress& r){
        return !memcmp(data, r.data, 6);
    }

    bool operator!=(const MACAddress& r){
        return !operator==(r);
    }

    uint8_t& operator[](unsigned pos){
        return data[pos];
    }
} __attribute__((packed)) mac_address_t;

struct EthernetFrame {
    MACAddress dest; // Destination MAC Address
    MACAddress src; // Source MAC Address
    BigEndianUInt16 etherType;
    uint8_t data[];
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

struct UDPHeader {
    BigEndianUInt16 srcPort;
    BigEndianUInt16 destPort;
    BigEndianUInt16 length;
    BigEndianUInt16 checksum;
    uint8_t data[];
} __attribute__((packed));

struct ICMPHeader{
    uint8_t type;
    uint8_t code;
    BigEndianUInt16 checksum;
    uint8_t data[];
} __attribute__((packed));

namespace Network {
    enum {
        EtherTypeIPv4 = 0x800,
        EtherTypeARP = 0x806,
    };

    enum {
        IPv4ProtocolICMP = 0x1,
        IPv4ProtocolTCP = 0x6,
        IPv4ProtocolUDP = 0x11,
    };

    static inline BigEndianUInt16 CaclulateChecksum(void* data, size_t size){
        uint16_t* ptr = (uint16_t*)data;
        size_t count = sizeof(IPv4Header);
        uint32_t checksum = 0;

        while(count >= 2){
            checksum += *ptr++;
            count -= 2;
        }

        checksum = (checksum & 0xFFFF) + (checksum >> 16);
        checksum += checksum >> 16;

        BigEndianUInt16 ret;
        ret = ~checksum;
        return ret;
    }

    void InitializeDrivers();
    void InitializeConnections();
    
    unsigned short AllocatePort(Socket& sock);
    int AcquirePort(Socket& sock, unsigned short port);
    void ReleasePort(unsigned short port);

    namespace Interface {
        void Initialize();

        void Send(void* data, size_t length);
        int SendIPv4(void* data, size_t length, IPv4Address& destination, uint8_t protocol);
        int SendUDP(void* data, size_t length, IPv4Address& destination, BigEndianUInt16 sourcePort, BigEndianUInt16 destinationPort);
    }
    
}