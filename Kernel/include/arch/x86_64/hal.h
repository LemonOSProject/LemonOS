#pragma once

#include <mischdr.h>

namespace HAL{
    extern memory_info_t mem_info;
    extern video_mode_t videoMode;
    extern multiboot2_info_header_t* multibootInfo;
    extern uintptr_t multibootModulesAddress;
    extern int bootModuleCount;
    extern boot_module_t bootModules[];
    extern bool debugMode;
    extern bool disableSMP;
    extern bool useKCon;

    void InitCore(multiboot2_info_header_t* mb_info);

    void InitVideo();

    void InitExtra();

    void Init(multiboot2_info_header_t* mb_info);
}