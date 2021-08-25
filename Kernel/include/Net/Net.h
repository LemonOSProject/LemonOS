#pragma once

#include <Net/If.h>

#include <CString.h>
#include <Device.h>
#include <Endian.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <stdint.h>

class Socket;

#define PORT_MAX UINT16_MAX

#define EPHEMERAL_PORT_RANGE_START 49152U
#define EPHEMERAL_PORT_RANGE_END PORT_MAX

#define ETHERNET_MAX_PACKET_SIZE 1518

#define TCP_RETRY_MIN 200000   // 200 ms minimum retry period
#define TCP_RETRY_MAX 32000000 // 32s

namespace Network {
class NetworkAdapter;
}
struct NetworkPacket {
    size_t length;
    uint8_t data[1518];

    Network::NetworkAdapter* adapter;

    NetworkPacket* next;
    NetworkPacket* prev;
};

struct IPv4Address {
    union {
        struct {
            uint8_t data[4];
        };
        uint32_t value;
    };

    IPv4Address() {}

    IPv4Address(uint8_t one, uint8_t two, uint8_t three, uint8_t four) {
        data[0] = one;
        data[1] = two;
        data[2] = three;
        data[3] = four;
    }

    IPv4Address(const uint32_t address) { *((uint32_t*)data) = address; }

    IPv4Address& operator=(const uint8_t newData[]) {
        memcpy(data, newData, 4);

        return *this;
    }

    IPv4Address& operator=(const uint32_t address) {
        *((uint32_t*)data) = address;

        return *this;
    }

} __attribute__((packed));

struct IPv4Header {               // Keep in mind that our architecture is little endian so the bitfields are swapped
    uint8_t ihl : 4;              // Internet Header Length
    uint8_t version : 4;          // Should be 4
    uint8_t ecn : 2;              // Explicit Congestion Notification
    uint8_t dscp : 6;             // Differentiated Services Code Point
    BigEndian<uint16_t> length;   // Total Length - Packet size in bytes including header
    BigEndian<uint16_t> id;       // ID
    uint16_t fragmentOffset : 13; // Offset of the fragment
    uint16_t flags : 3;           // 0 - Reserved, 1 - Don't Fragment, 2 - More Fragments
    uint8_t ttl;                  // Time to live
    uint8_t protocol;             // Protocol
    BigEndian<uint16_t> headerChecksum; // Checksum
    IPv4Address sourceIP;               // Source IP Address
    IPv4Address destIP;                 // Destination IP Address
    uint8_t data[];

    IPv4Header() = default;
    inline IPv4Header(const IPv4Address& src, const IPv4Address& dest, uint8_t proto, uint16_t len) {
        ihl = 5;     // 5 dwords (20 bytes)
        version = 4; // Internet Protocol version 4
        length = len + sizeof(IPv4Header);
        flags = 0;
        ttl = 64;
        protocol = proto;
        headerChecksum = 0;
        destIP = dest;
        sourceIP = src;
    }

} __attribute__((packed));

typedef struct MACAddress {
    uint8_t data[6];

    inline constexpr MACAddress& operator=(const uint8_t* const newData) {
        data[0] = newData[0];
        data[1] = newData[1];
        data[2] = newData[2];
        data[3] = newData[3];
        data[4] = newData[4];
        data[5] = newData[5];

        return *this;
    }

    inline bool operator==(const MACAddress& r) const { return !memcmp(data, r.data, 6); }

    inline bool operator!=(const MACAddress& r) const { return !operator==(r); }

    uint8_t& operator[](unsigned pos) {
        assert(pos < 6);

        return data[pos];
    }

    uint8_t operator[](unsigned pos) const {
        assert(pos < 6);

        return data[pos];
    }
} __attribute__((packed)) mac_address_t;

struct EthernetFrame {
    MACAddress dest; // Destination MAC Address
    MACAddress src;  // Source MAC Address
    BigEndian<uint16_t> etherType;
    uint8_t data[];
} __attribute__((packed));

struct ARPHeader {
    enum {
        ARPRequest = 1,
        ARPReply = 2,
    };

    BigEndian<uint16_t> hwType; // Hardware Type
    BigEndian<uint16_t> prType; // Protocol Type
    uint8_t hLength = 6;        // Hardware Address Length
    uint8_t pLength = 4;        // Protocol Address Length
    BigEndian<uint16_t> opcode; // ARP Operation Code
    MACAddress srcHwAddr;       // Source hardware address
    IPv4Address srcPrAddr;      // Source protocol address
    MACAddress destHwAddr;      // Destination hardware address
    IPv4Address destPrAddr;     // Destination protocol address
} __attribute__((packed));

struct UDPHeader {
    BigEndian<uint16_t> srcPort;
    BigEndian<uint16_t> destPort;
    BigEndian<uint16_t> length;
    BigEndian<uint16_t> checksum;
    uint8_t data[];
} __attribute__((packed));

struct TCPHeader {
    enum Flags {
        FIN = 0x1,
        SYN = 0x2,
        RST = 0x4,
        PSH = 0x8,
        ACK = 0x10,
        URG = 0x20,
        ECE = 0x40,
        CWR = 0x80,
        NS = 0x100,
        FlagsMask = 0x1ff,
    };

    BigEndian<uint16_t> srcPort;               // Source Port
    BigEndian<uint16_t> destPort;              // Destination Port
    BigEndian<uint32_t> sequence;              // Sequence Number
    BigEndian<uint32_t> acknowledgementNumber; // Acknowledgement Number
    union {
        struct {
            uint8_t ns : 1; // Nonce
            uint8_t reserved : 3;
            uint8_t dataOffset : 4; // Size of the tcp header in DWORDs (min 5, max 15)
            uint8_t fin : 1;        // Last packet from sender
            uint8_t syn : 1;        // Synchronize sequence numbers, should be sent at start of transmission
            uint8_t rst : 1;        // Reset the connection
            uint8_t psh : 1;        // Push function, push buffered data to the recieving application
            uint8_t ack : 1; // Acknowledgement number used, all packets after the first SYN should have this flag set
            uint8_t urg : 1; // Urgent pointer used
            uint8_t
                ece : 1; // If SYN is set: ECN capable, if SYN clear: packet with Congestion Experienced set received
            uint8_t cwr : 1; // Congestion window reduced
        };
        BigEndian<uint16_t> flags = 0;
    };
    BigEndian<uint16_t> windowSize = 0;
    BigEndian<uint16_t> checksum; // Checksum of TCP header, payload and IP pseudoheader consisting of source IP, dest
                                  // IP, length and protocol number
    BigEndian<uint16_t> urgentPointer = 0; // If URG set, offset from the sequence number indicating last urgent byte
    // Options...
} __attribute__((packed));
static_assert(!(sizeof(TCPHeader) & (sizeof(uint32_t) - 1)));

struct ICMPHeader {
    uint8_t type;
    uint8_t code;
    BigEndian<uint16_t> checksum;
    uint8_t data[];
} __attribute__((packed));

namespace Network {
extern Semaphore packetQueueSemaphore;

enum {
    EtherTypeIPv4 = 0x800,
    EtherTypeARP = 0x806,
};

enum {
    IPv4ProtocolICMP = 0x1,
    IPv4ProtocolTCP = 0x6,
    IPv4ProtocolUDP = 0x11,
};

class NetFS : public Device {
private:
    static NetFS* instance;

public:
    NetFS();

    int ReadDir(DirectoryEntry* dirent, uint32_t index);
    FsNode* FindDir(const char* name);

    void RegisterAdapter(NetworkAdapter* adapter);
    void RemoveAdapter(NetworkAdapter* adapter);

    NetworkAdapter* FindAdapter(const char* name, size_t nameLen);
    NetworkAdapter* FindAdapter(uint32_t ip);

    inline static NetFS* GetInstance() { return instance; }
};

static inline BigEndian<uint16_t> CaclulateChecksum(void* data, uint16_t size) {
    uint16_t* ptr = (uint16_t*)data;
    size_t count = size;
    uint32_t checksum = 0;

    while (count >= 2) {
        checksum += *ptr++;
        count -= 2;
    }

    checksum = (checksum & 0xFFFF) + (checksum >> 16);
    if (checksum > UINT16_MAX) {
        checksum += 1;
    }

    BigEndian<uint16_t> ret;
    ret.value = ~checksum;
    return ret;
}

void InitializeConnections();

int IPLookup(NetworkAdapter* adapter, const IPv4Address& ip, MACAddress& mac);
int Route(const IPv4Address& local, const IPv4Address& dest, MACAddress& mac, NetworkAdapter*& adapter);

void InitializeNetworkThread();

void Send(void* data, size_t length, NetworkAdapter* adapter = nullptr);
int SendIPv4(void* data, size_t length, IPv4Address& source, IPv4Address& destination, uint8_t protocol,
             NetworkAdapter* adapter = nullptr);

namespace UDP {
class UDPSocket;

int SendUDP(void* data, size_t length, IPv4Address& source, IPv4Address& destination, BigEndian<uint16_t> sourcePort,
            BigEndian<uint16_t> destinationPort, NetworkAdapter* adapter = nullptr);
void OnReceiveUDP(IPv4Header& ipHeader, void* data, size_t length);
} // namespace UDP

namespace TCP {
class TCPSocket;

BigEndian<uint16_t> CalculateTCPChecksum(const IPv4Address& src, const IPv4Address& dest, void* data, uint16_t size);

int SendTCP(void* data, size_t length, IPv4Address& source, IPv4Address& destination, BigEndian<uint16_t> sourcePort,
            BigEndian<uint16_t> destinationPort, NetworkAdapter* adapter = nullptr);
void OnReceiveTCP(IPv4Header& ipHeader, void* data, size_t length);
} // namespace TCP
} // namespace Network