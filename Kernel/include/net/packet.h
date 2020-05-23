#pragma once

#include <stddef.h>

typedef struct NetworkPacket {
    void* data;
    size_t length;
} network_packet_t;