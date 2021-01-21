#pragma once

#include <stdint.h>

#include "endian.h"

#define DHCP_FLAG_BROADCAST 0x1

#define DHCP_MAGIC_COOKIE   0x63825363

enum {
    DHCPHardwareTypeEthernet = 1,
};

enum {
    DHCPOpCodeDiscover = 1,
    DHCPOpCodeOffset = 2,
};

enum {
    DHCPOptionSubnetMask = 1, // Request Subnet Mask
    DHCPOptionDefaultGateway = 3, // Request gateway
    DHCPOptionDNS = 6, // Request DNS servers
    DHCPOptionRequestIPAddress = 50, // Request a specific IP address
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
    BigEndianUInt16 secs; // Number of seconds since the process has started
    BigEndianUInt16 flags; // Flags
    uint32_t clientIP; // Client's IP Address if known
    uint32_t yourIP; // IP Address assigned to client
    uint32_t serverIP; // Server IP Address
    uint32_t gatewayIP; // Gateway IP Address
    uint8_t clientAddress[16]; // Client Hardware Address
    int8_t serverName[64]; // Server name
    uint8_t bootFilename[128];
    BigEndianUInt32 cookie;
    uint8_t options[312];
} __attribute__((packed));

template<uint8_t optlen>
struct DHCPOption{
    uint8_t optionCode;
    uint8_t length = optlen;
    uint8_t data[optlen];
} __attribute__((packed));