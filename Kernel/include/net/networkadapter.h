#pragma once

#include <device.h>

enum {
    LinkDown,
    LinkUp,
};

class NetworkAdapter : public Device {
    int linkState = LinkDown;

public:
    
};