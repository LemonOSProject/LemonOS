#pragma once

#include <stdint.h>
#include <endian.h>
#include <net/net.h>

#define DHCP_FLAG_BROADCAST 0x1

enum {
    DHCPOptionRequestIPAddress = 50,
    DHCPOptionIPLeaseTime = 51,
    DHCPOptionOverload = 52,
    DHCPOptionMessageType = 53,
    DHCPOptionServerIdentifier = 54,
    DHCPOptionParameterRequestList = 55,
};

enum {
    DHCPMessageTypeDiscover = 1,
    DHCPMessageTypeOffer = 2,
    DHCPMessageTypeRequest = 3,
    DHCPMessageTypeDecline = 4,
    DHCPMessageTypeAcknowledge = 5,
    DHCPMessageTypeNotAcknowledged = 6,
    DHCPMessageTypeRelease = 7,
    DHCPMessageTypeInform = 8,
};

struct DHCPHeader {
    uint8_t op; // Operation code
    uint8_t htype; // Hardware type (1 = Ethernet)
    uint8_t hlen; // Length of hardware addresses (6 = MAC)
    uint8_t hops;
    BigEndianUInt32 xID; // Transmission ID
    BigEndianUInt16 secs;
    BigEndianUInt16 flags; // Flags
    IPv4Address clientIP; // Client's IP Address if known
    IPv4Address yourIP; // IP Address assigned to client
    IPv4Address serverIP; // Server IP Address
    IPv4Address gatewayIP; // Gateway IP Address
    uint8_t clientAddress[16]; // Client Hardware Address
    int8_t serverName[64]; // Server name
    uint8_t bootFilename[128];
    BigEndianUInt32 cookie;
    uint8_t options[16];
} __attribute__((packed));