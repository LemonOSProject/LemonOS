#include <hal.h>

#include <string.h>
#include <serial.h>
#include <logging.h>
#include <video.h>
#include <idt.h>
#include <paging.h>
#include <physicalallocator.h>
#include <pci.h>
#include <acpi.h>
#include <timer.h>

namespace HAL{
    memory_info_t mem_info;
    video_mode_t videoMode;
    multiboot_info_t multibootInfo;
    uintptr_t multibootModulesAddress;

    void InitCore(multiboot_info_t mb_info){ // ALWAYS call this first
        multibootInfo = mb_info;

        // Check if Debugging Mode is enabled and if so initialize serial port
        initialize_serial();
        Log::Info("Initializing Lemon...\r\n");

        // Initialize GDT and IDT
        IDT::Initialize();

        // Initialize Paging/Virtual Memory Manager
        Memory::InitializeVirtualMemory();
        Log::Info(mb_info.mmapAddr);

        multiboot_memory_map_t* memory_map = (multiboot_memory_map_t*)(mb_info.mmapAddr + KERNEL_VIRTUAL_BASE);

        // Initialize Memory Info Structure to pass to Physical Memory Allocator
        mem_info.memory_high = mb_info.memoryHi;
        mem_info.memory_low = mb_info.memoryLo;
        mem_info.mem_map = memory_map;
        mem_info.memory_map_len = mb_info.mmapLength;

        Log::Info((uint64_t)Memory::KernelAllocate4KPages(1));
        Log::Info((uint64_t)Memory::KernelAllocate4KPages(1));
        Log::Info((uint64_t)Memory::KernelAllocate4KPages(1));
        Log::Info((uint64_t)Memory::KernelAllocate4KPages(1));

        uint64_t mbModsVirt = (uint64_t)Memory::KernelAllocate4KPages(1);
        Memory::KernelMapVirtualMemory4K(multibootInfo.modsAddr, mbModsVirt, 1);

        multibootModulesAddress = mbModsVirt;

        // Initialize Physical Memory Allocator
        Memory::InitializePhysicalAllocator(&mem_info);
		Log::Info("aaa");

        Log::Info("Initializing System Timer...");
        Timer::Initialize(1200);
        Log::Write("OK");

        Log::Info("Initializing PCI...");
        //PCI::Init();
        Log::Write("OK");

        Log::Info("Initializing ACPI...");
        //ACPI::Init();
        Log::Write("OK");
    } 

    void InitVideo(){
        // Map Video Memory
        uint64_t vidMemSize = multibootInfo.framebufferHeight*multibootInfo.framebufferPitch;
        
        void* vidMemVirt = Memory::KernelAllocate4KPages(vidMemSize / PAGE_SIZE_4K + 1);
        Memory::KernelMapVirtualMemory4K(multibootInfo.framebufferAddr, (uint64_t)vidMemVirt, vidMemSize / PAGE_SIZE_4K + 1);
        Memory::MarkMemoryRegionUsed(multibootInfo.framebufferAddr,vidMemSize);

        // Initialize Video Mode structure
        videoMode.width = multibootInfo.framebufferWidth;
        videoMode.height = multibootInfo.framebufferHeight;
        videoMode.bpp = multibootInfo.framebufferBpp;
        videoMode.pitch = multibootInfo.framebufferPitch;
        videoMode.address = vidMemVirt;

        Video::Initialize(videoMode);
        Video::DrawRect(0,0,videoMode.width,videoMode.height, 0,0,255);
        Video::DrawString("Starting Lemon x64...", 0, 0, 255, 255, 255);
    }

    void InitExtra(){
    }

    void Init(multiboot_info_t mb_info){
        InitCore(mb_info);
        InitVideo();
        InitExtra();
    }
}
