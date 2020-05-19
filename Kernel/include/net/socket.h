#pragma once

#include <stdint.h>

typedef unsigned short sa_family_t;
typedef uint32_t socklen_t;

typedef struct sockaddr {
    sa_family_t family;
    char data[14];
} sockaddr_t;