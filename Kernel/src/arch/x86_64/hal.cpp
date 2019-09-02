#include <hal.h>

#include <string.h>
#include <serial.h>
#include <logging.h>
#include <video.h>
#include <idt.h>
#include <paging.h>
#include <physicalallocator.h>

namespace HAL{
    memory_info_t mem_info;
    video_mode_t videoMode;

    multiboot_info_t multibootInfo;
    void InitCore(multiboot_info_t mb_info){ // ALWAYS call this first
        multibootInfo = mb_info;

        // Check if Debugging Mode is enabled and if so initialize serial port
        initialize_serial();
        //Log::Info("Initializing Lemon...\r\n");
        write_serial("Initializing Lemon x86_64...\r\n");

        // Initialize GDT and IDT
        IDT::Initialize();

        // Initialize Paging/Virtual Memory Manager
        Memory::InitializeVirtualMemory();

        // Allocate virtual memory for memory map
        uint64_t mmap_virt = (uint64_t)Memory::KernelAllocate2MPages(mb_info.mmapLength / PAGE_SIZE_2M + 1);
        // Get Memory Map
        Memory::KernelMap2MPages(mb_info.mmapAddr, mmap_virt, mb_info.mmapLength / PAGE_SIZE_2M + 1);
        multiboot_memory_map_t* memory_map = (multiboot_memory_map_t*)mb_info.mmapAddr;

        // Initialize Memory Info Structure to pass to Physical Memory Allocator
        mem_info.memory_high = mb_info.memoryHi;
        mem_info.memory_low = mb_info.memoryLo;
        mem_info.mem_map = memory_map;
        mem_info.memory_map_len = mb_info.mmapLength;

        //uint64_t mbModsVirt = Memory::KernelAllocateVirtualPages(1);
        //Memory::MapVirtualPages(multibootInfo.modsAddr, mbModsVirt, 1);
        //multibootInfo.modsAddr = mbModsVirt;

        // Initialize Physical Memory Allocator
        Memory::InitializePhysicalAllocator(&mem_info);
    } 

    void InitVideo(){
        // Map Video Memory
        uint64_t vidMemSize = multibootInfo.framebufferHeight*multibootInfo.framebufferPitch;
        
        void* vidMemVirt = Memory::KernelAllocate2MPages(vidMemSize / PAGE_SIZE_2M + 1);
        Memory::KernelMap2MPages(multibootInfo.framebufferAddr, (uint64_t)vidMemVirt, vidMemSize / PAGE_SIZE_2M + 1);

        // Initialize Video Mode structure
        videoMode.width = multibootInfo.framebufferWidth;
        videoMode.height = multibootInfo.framebufferHeight;
        videoMode.bpp = multibootInfo.framebufferBpp;
        videoMode.pitch = multibootInfo.framebufferPitch;
        videoMode.address = vidMemVirt;

//Log::Info(multibootInfo.framebufferAddr);
//Log::Info((uint64_t)vidMemVirt);

        Video::Initialize(videoMode);
        Video::DrawRect(0,0,videoMode.width,videoMode.height, 0,0,255);
        Video::DrawString("Starting Lemon x64...", 0, 0, 255, 255, 255);
    }

    void InitExtra(){
        // Intialize Keyboard

        // Initalize Mouse
    }

    void Init(multiboot_info_t mb_info){
        InitCore(mb_info);
        InitVideo();
        InitExtra();
    }
}
