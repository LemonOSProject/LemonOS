#pragma once

#include <net/if.h>

#include <stdint.h>
#include <stddef.h>
#include <endian.h>
#include <string.h>
#include <device.h>

class Socket;

#define PORT_MAX UINT16_MAX

#define EPHEMERAL_PORT_RANGE_START 49152U
#define EPHEMERAL_PORT_RANGE_END PORT_MAX

#define ETHERNET_MAX_PACKET_SIZE 1518

namespace Network{ class NetworkAdapter; }
struct NetworkPacket{
    size_t length;
    uint8_t data[1518];

    Network::NetworkAdapter* adapter;

    NetworkPacket* next;
    NetworkPacket* prev;
};

struct IPv4Address{
    union{
        struct{
            uint8_t data[4];
        };
        uint32_t value;
    };

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

        return *this;
    }

    IPv4Address& operator=(const uint32_t address){
        *((uint32_t*)data) = address;

        return *this;
    }
} __attribute__((packed));

struct IPv4Header{ // Keep in mind that our architecture is little endian so the bitfields are swapped
    uint8_t ihl : 4; // Internet Header Length
    uint8_t version : 4; // Should be 4
    uint8_t ecn : 2; // Explicit Congestion Notification
    uint8_t dscp : 6; // Differentiated Services Code Point
    BigEndian<uint16_t> length; // Total Length - Packet size in bytes including header
    BigEndian<uint16_t> id; // ID
    uint16_t fragmentOffset : 13; // Offset of the fragment
    uint16_t flags : 3; // 0 - Reserved, 1 - Don't Fragment, 2 - More Fragments
    uint8_t ttl; // Time to live
    uint8_t protocol; // Protocol
    BigEndian<uint16_t> headerChecksum; // Checksum
    IPv4Address sourceIP; // Source IP Address
    IPv4Address destIP; // Destination IP Address
    uint8_t data[];
} __attribute__((packed));

typedef struct MACAddress {
    uint8_t data[6];

    MACAddress& operator=(const uint8_t newData[]){
        memcpy(data, newData, 6);

        return *this;
    }

    bool operator==(const MACAddress& r) const{
        return !memcmp(data, r.data, 6);
    }

    bool operator!=(const MACAddress& r) const{
        return !operator==(r);
    }

    uint8_t& operator[](unsigned pos){
        return data[pos];
    }
} __attribute__((packed)) mac_address_t;

struct EthernetFrame {
    MACAddress dest; // Destination MAC Address
    MACAddress src; // Source MAC Address
    BigEndian<uint16_t> etherType;
    uint8_t data[];
} __attribute__((packed));

struct ARPHeader {
    BigEndian<uint16_t> hwType; // Hardware Type
    BigEndian<uint16_t> prType; // Protocol Type
    uint8_t hLength = 6; // Hardware Address Length
    uint8_t pLength = 4; // Protocol Address Length
    BigEndian<uint16_t> opcode; // ARP Operation Code
    MACAddress srcHwAddr; // Source hardware address
    IPv4Address srcPrAddr; // Source protocol address
    MACAddress destHwAddr; // Destination hardware address
    IPv4Address destPrAddr; // Destination protocol address
} __attribute__((packed));

struct UDPHeader {
    BigEndian<uint16_t> srcPort;
    BigEndian<uint16_t> destPort;
    BigEndian<uint16_t> length;
    BigEndian<uint16_t> checksum;
    uint8_t data[];
} __attribute__((packed));

struct ICMPHeader{
    uint8_t type;
    uint8_t code;
    BigEndian<uint16_t> checksum;
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

    class NetFS : public Device{
    private:
        static NetFS* instance;

    public:
        NetFS();

        int ReadDir(DirectoryEntry* dirent, uint32_t index);
        FsNode* FindDir(char* name);

        void RegisterAdapter(NetworkAdapter* adapter);
        NetworkAdapter* FindAdapter(const char* name, size_t nameLen);
        NetworkAdapter* FindAdapter(uint32_t ip);

        inline static NetFS* GetInstance() { return instance; }
    };

    static inline BigEndian<uint16_t> CaclulateChecksum(void* data, size_t size){
        uint16_t* ptr = (uint16_t*)data;
        size_t count = size;
        uint32_t checksum = 0;

        while(count >= 2){
            checksum += *ptr++;
            count -= 2;
        }

        checksum = (checksum & 0xFFFF) + (checksum >> 16);
        if(checksum > UINT16_MAX){
            checksum += 1;
        }

        BigEndian<uint16_t> ret;
        ret.value = ~checksum;
        return ret;
    }

    void InitializeDrivers();
    void InitializeConnections();
    
    unsigned short AllocatePort(Socket& sock);
    int AcquirePort(Socket& sock, unsigned int port);
    void ReleasePort(unsigned short port);

    namespace Interface {
        void Initialize();

        void Send(void* data, size_t length, NetworkAdapter* adapter = nullptr);
        int SendIPv4(void* data, size_t length, IPv4Address& destination, uint8_t protocol, NetworkAdapter* adapter = nullptr);
        int SendUDP(void* data, size_t length, IPv4Address& destination, BigEndian<uint16_t> sourcePort, BigEndian<uint16_t> destinationPort, NetworkAdapter* adapter = nullptr);
    }
    
    namespace UDP{
        Socket* FindSocket(BigEndian<uint16_t> port);
        void OnReceiveUDP(IPv4Header& ipHeader, void* data, size_t length);
    }
}