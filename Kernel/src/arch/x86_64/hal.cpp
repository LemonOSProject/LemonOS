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
#include <smp.h>
#include <videoconsole.h>

extern void* _end;

namespace HAL{
    memory_info_t mem_info;
    video_mode_t videoMode;
    multiboot2_info_header_t* multibootInfo;
    uintptr_t multibootModulesAddress;
    boot_module_t bootModules[128];
    int bootModuleCount;
    bool debugMode = false;
    bool disableSMP = false;
    bool useKCon = false;
    VideoConsole* con;

    void InitCore(multiboot2_info_header_t* mbInfo){ // ALWAYS call this first
        // Check if Debugging Mode is enabled and if so initialize serial port
        initialize_serial();
        Log::Info("Initializing Lemon...\r\n");

        // Initialize IDT
        IDT::Initialize();

        // Initialize Paging/Virtual Memory Manager
        Memory::InitializeVirtualMemory();
        
        //multibootModulesAddress = Memory::GetIOMapping(multibootInfo.modsAddr); // Grub loads the kernel as 32-bit so modules will be <4GB

        char* cmdLine = nullptr;
        
        asm("cli");

        // Initialize Physical Memory Allocator
        Memory::InitializePhysicalAllocator(&mem_info);

        multiboot2_tag_t* tag = reinterpret_cast<multiboot2_tag_t*>(mbInfo->tags);

        multiboot2_module_t* modules[128];
        unsigned bootModuleCount = 0;

        while(tag->type && reinterpret_cast<uintptr_t>(tag) < reinterpret_cast<uintptr_t>(mbInfo) + mbInfo->totalSize){
            switch(tag->type){
                case Mboot2CmdLine: {
                    multiboot2_cmdline_t* mbCmdLine = reinterpret_cast<multiboot2_cmdline_t*>(tag);

                    cmdLine = mbCmdLine->string;
                    break;
                }
                case Mboot2Modules:{
                    if(bootModuleCount >= 128){
                        Log::Warning("Exceeded maximum amount of boot modules!");
                        break;
                    }

                    multiboot2_module_t* mod = reinterpret_cast<multiboot2_module_t*>(tag);

                    modules[bootModuleCount++] = mod;
                }
                case Mboot2MemoryInfo: {
                    multiboot2_memory_info_t* mbMemInfo = reinterpret_cast<multiboot2_memory_info_t*>(tag);
                    Log::Info("Bootloader reports %d MB of memory", (mbMemInfo->memoryLower + mbMemInfo->memoryUpper) / 1024);
                    mem_info.memory_high = mbMemInfo->memoryUpper;
                    mem_info.memory_low = mbMemInfo->memoryLower;
                    break;
                } 
                case Mboot2MemoryMap: {
                    multiboot2_memory_map_t* mbMemMap = reinterpret_cast<multiboot2_memory_map_t*>(tag); 
                    
                    multiboot2_mmap_entry_t* currentEntry = mbMemMap->entries;
                    while(currentEntry < reinterpret_cast<void*>(mbMemMap) + mbMemMap->size){
                        switch (currentEntry->type)
                        {
                        case 1: // Available
                            Log::Info("Memory region [%x-%x] available", currentEntry->base, currentEntry->base + currentEntry->length);
                            Memory::MarkMemoryRegionFree(currentEntry->base, currentEntry->length);
                            break;
                        default: // Not available
                            Log::Info("Memory region [%x-%x] reserved", currentEntry->base, currentEntry->base + currentEntry->length);
                            break;
                        }
                        currentEntry = reinterpret_cast<multiboot2_mmap_entry_t*>((uintptr_t)currentEntry + mbMemMap->entrySize);
                    }
                }
                case Mboot2FramebufferInfo: {
                    multiboot2_framebuffer_info_t* mbFbInfo = reinterpret_cast<multiboot2_framebuffer_info_t*>(tag);

                    videoMode.address = reinterpret_cast<void*>(Memory::GetIOMapping(mbFbInfo->framebufferAddr));
                    videoMode.width = mbFbInfo->framebufferWidth;
                    videoMode.height = mbFbInfo->framebufferHeight;
                    videoMode.pitch = mbFbInfo->framebufferPitch;
                    videoMode.bpp = mbFbInfo->framebufferBpp;

                    videoMode.physicalAddress = mbFbInfo->framebufferAddr;

                    videoMode.type = mbFbInfo->type;
                }
                case Mboot2ACPI1RSDP: {
                    auto rsdp = &(reinterpret_cast<multiboot2_acpi1_rsdp_t*>(tag)->rsdp);
                    ACPI::SetRSDP(rsdp);
                }
                default: {
                    Log::Info("Ignoring boot tag %d", tag->type);
                    break;
                }
            }

            tag = tag->next(); // Get next tag
        }

        Memory::MarkMemoryRegionUsed(0, (uintptr_t)&_end - KERNEL_VIRTUAL_BASE); // Make sure kernel memory is marked as used
        
        if(cmdLine){
            cmdLine = strtok((char*)cmdLine, " ");
            
            while(cmdLine){
                if(strcmp(cmdLine, "debug") == 0) debugMode = true;
                else if(strcmp(cmdLine, "nosmp") == 0) disableSMP = true;
                else if(strcmp(cmdLine, "kcon") == 0) useKCon = true;
                cmdLine = strtok(NULL, " ");
            }
        }

        asm("sti");
        Log::Initialize();

        // Manage Multiboot Modules
	    Log::Info("Multiboot Module Count: %d", bootModuleCount);

        for(unsigned i = 0; i < bootModuleCount; i++){
            multiboot2_module_t& mod = *modules[i];
            Log::Info("    Multiboot Module %d [Start: %x, End: %x, Cmdline: %s]", i, mod.moduleStart, mod.moduleEnd, mod.string);
            Memory::MarkMemoryRegionUsed(mod.moduleStart, mod.moduleEnd);
            bootModules[i] = {
                .base = Memory::GetIOMapping(mod.moduleStart),
                .size = mod.moduleEnd - mod.moduleStart,
            };
            
        }

        Log::Info("Initializing System Timer...");
        Timer::Initialize(1000);
        Log::Write("OK");
    } 

    void InitVideo(){
        Video::Initialize(videoMode);
        Video::DrawString("Starting Lemon x64...", 0, 0, 255, 255, 255);

        Log::SetVideoConsole(NULL);

        if(debugMode){
            con = new VideoConsole(0, (videoMode.height / 3) * 2, videoMode.width, videoMode.height / 3);
            Log::SetVideoConsole(con);
        }
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
        
        Log::Info("Initializing SMP...");
        SMP::Initialize();
        Log::Write("OK");
    }

    void Init(multiboot2_info_header_t* mb_info){
        InitCore(mb_info);
        InitVideo();
        InitExtra();
    }
}
