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
#include <tss.h>
#include <apic.h>
#include <liballoc.h>

namespace HAL{
    memory_info_t mem_info;
    video_mode_t videoMode;
    multiboot_info_t multibootInfo;
    uintptr_t multibootModulesAddress;
    boot_module_t bootModules[32];
    int bootModuleCount;

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
        
        multibootModulesAddress = Memory::GetIOMapping(multibootInfo.modsAddr); // Grub loads the kernel as 32-bit so modules will be <4GB
        
        asm("cli");

        // Initialize Physical Memory Allocator
        Memory::InitializePhysicalAllocator(&mem_info);

        asm("sti");

        // Manage Multiboot Modules
	    Log::Info("Multiboot Module Count: %d", multibootInfo.modsCount);
        bootModuleCount = multibootInfo.modsCount;

        for(unsigned i = 0; i < multibootInfo.modsCount; i++){
            multiboot_module_t mod = *((multiboot_module_t*)multibootModulesAddress + i * sizeof(multiboot_module_t));
            Log::Info("    Multiboot Module %d [Start: %x, End: %x, Cmdline: %s]", i, mod.mod_start, mod.mod_end, (char*)Memory::GetIOMapping(mod.string));
            Memory::MarkMemoryRegionUsed(mod.mod_start, mod.mod_end);
            bootModules[i] = {
                .base = Memory::GetIOMapping(mod.mod_start),
                .size = mod.mod_end - mod.mod_start,
            };
            
        }

        // Initialize Task State Segment (and Interrupt Stack Tables)
        TSS::Initialize();

        Log::Info("Initializing System Timer...");
        Timer::Initialize(1000);
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
        Video::DrawString("Starting Lemon x64...", 0, 0, 255, 255, 255);
    }

    void InitExtra(){
        Log::Info("Initializing PCI...");
        PCI::Init();
        Log::Write("OK");

        Log::Info("Initializing ACPI...");
        ACPI::Init();
        Log::Write("OK");
        
        Log::Info("Initializing Local and I/O APIC...");
        APIC::Initialize();
        Log::Write("OK");
    }

    void Init(multiboot_info_t mb_info){
        InitCore(mb_info);
        InitVideo();
        InitExtra();
    }
}
