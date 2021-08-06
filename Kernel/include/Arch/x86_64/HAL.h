#pragma once

#include <MiscHdr.h>

namespace HAL{
    extern memory_info_t mem_info;
    extern video_mode_t videoMode;
    extern uintptr_t multibootModulesAddress;
    extern int bootModuleCount;
    extern boot_module_t bootModules[];
    extern bool debugMode;
    extern bool disableSMP;
    extern bool useKCon;
    extern bool runTests;

    void InitCore();

    void InitVideo();
    void InitExtra();
}