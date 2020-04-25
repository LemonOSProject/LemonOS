#pragma once

#include <device.h>
#include <list.h>

namespace DeviceManager{
    extern List<Device*>* devices;

    void Init();
}