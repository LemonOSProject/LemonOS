#include <hal.h>

#include <string.h>
#include <serial.h>
#include <logging.h>
#include <video.h>
#include <idt.h>

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
        /*
        // Allocate virtual memory for memory map
        uint32_t mmap_virt = Memory::KernelAllocateVirtualPages(mb_info.mmapLength / PAGE_SIZE + 1);

        // Get Memory Map
        Memory::MapVirtualPages(mb_info.mmapAddr, mmap_virt, mb_info.mmapLength / PAGE_SIZE + 1);
        multiboot_memory_map_t* memory_map = (multiboot_memory_map_t*)mb_info.mmapAddr;

        // Initialize Memory Info Structure to pass to Physical Memory Allocator
        mem_info.memory_high = mb_info.memoryHi;
        mem_info.memory_low = mb_info.memoryLo;
        mem_info.mem_map = memory_map;
        mem_info.memory_map_len = mb_info.mmapLength;

        uint32_t mbModsVirt = Memory::KernelAllocateVirtualPages(1);
        Memory::MapVirtualPages(multibootInfo.modsAddr, mbModsVirt, 1);
        //multibootInfo.modsAddr = mbModsVirt;

        // Initialize Physical Memory Allocator
        Memory::InitializePhysicalAllocator(&mem_info);*/

for(;;);
    } 

    void InitVideo(){
        // Map Video Memory
        /*uint32_t vidMemSize = multibootInfo.framebufferHeight*multibootInfo.framebufferPitch;
        
        uint32_t vidMemVirt = Memory::KernelAllocateVirtualPages(vidMemSize / PAGE_SIZE + 1);
        Memory::MapVirtualPages(multibootInfo.framebufferAddr, vidMemVirt, vidMemSize / PAGE_SIZE + 1);

        // Initialize Video Mode structure
        videoMode.width = multibootInfo.framebufferWidth;
        videoMode.height = multibootInfo.framebufferHeight;
        videoMode.bpp = multibootInfo.framebufferBpp;
        videoMode.pitch = multibootInfo.framebufferPitch;
        videoMode.address = vidMemVirt;

        Video::Initialize(videoMode);
        Video::DrawRect(0,0,videoMode.width,videoMode.height, 48,48,48);*/
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
