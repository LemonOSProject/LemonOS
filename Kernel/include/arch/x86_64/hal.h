#pragma once

#include <mischdr.h>

#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000

namespace HAL{
    extern memory_info_t mem_info;
    extern video_mode_t videoMode;
    extern multiboot_info_t multibootInfo;
    extern uintptr_t multibootModulesAddress;
    extern int bootModuleCount;
    extern boot_module_t bootModules[];

    void InitCore(multiboot_info_t mb_info);

    void InitVideo();

    void InitExtra();

    void Init(multiboot_info_t mb_info);
}