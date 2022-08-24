#include <HAL.h>

#include <ACPI.h>
#include <APIC.h>
#include <BootProtocols.h>
#include <CString.h>
#include <Device.h>
#include <IDT.h>
#include <Logging.h>
#include <MM/KMalloc.h>
#include <PCI.h>
#include <Paging.h>
#include <Panic.h>
#include <PhysicalAllocator.h>
#include <SMP.h>
#include <Serial.h>
#include <TSS.h>
#include <Timer.h>
#include <Video/Video.h>
#include <Video/VideoConsole.h>

#include <Debug.h>

extern void* _end;

namespace HAL {
memory_info_t mem_info;
video_mode_t videoMode;
multiboot2_info_header_t* multibootInfo;
uintptr_t multibootModulesAddress;
boot_module_t bootModules[128];
int bootModuleCount;
bool debugMode = false;
bool disableSMP = false;
bool useKCon = false;
bool runTests = false;
VideoConsole* con;

void InitMultiboot2(multiboot2_info_header_t* mbInfo);
void InitStivale2(stivale2_info_header_t* st2Info);

void InitCore() { // ALWAYS call this first
    asm volatile("cli");

    SMP::InitializeCPU0Context();
    
    Serial::Initialize();
    Serial::Write("Initializing Lemon...\r\n");

    // Initialize Paging/Virtual Memory Manager
    Memory::InitializeVirtualMemory();

    // Initialize IDT
    IDT::Initialize();

    // Initialize Physical Memory Allocator
    Memory::InitializePhysicalAllocator(&mem_info);
}

void InitVideo() {
    Video::Initialize(videoMode);
    Video::DrawString("Starting Lemon x64...", 0, 0, 255, 255, 255);

    Log::SetVideoConsole(NULL);

    if (debugMode) {
        con = new VideoConsole(0, (videoMode.height / 3) * 2, videoMode.width, videoMode.height / 3);
        Log::SetVideoConsole(con);
    }
}

void InitExtra() {
    Log::Info("Checking CPU supports x86_64-v2...");
    cpuid_info_t cpuidInfo = CPUID();

    if (!(cpuidInfo.features_ecx & CPUID_ECX_SSE4_2)) {
        KernelPanic("CPU does not support SSE4.2, Lemon OS requires x86_64-v2 support!");
    }

    if (!(cpuidInfo.features_ecx & CPUID_ECX_POPCNT)) {
        KernelPanic("CPU does not support POPCNT, Lemon OS requires x86_64-v2 support!");
    }

    if (!(cpuidInfo.features_ecx & CPUID_ECX_CX16)) {
        KernelPanic("CPU does not support CMPXCHG16, Lemon OS requires x86_64-v2 support!");
    }
    Log::Write("OK");

    Log::Info("Initializing ACPI...");
    ACPI::Init();
    Log::Write("OK");

    Log::Info("Initializing PCI...");
    PCI::Init();
    Log::Write("OK");

    Log::Info("Initializing System Timer...");
    Timer::Initialize(1600);
    Log::Write("OK");

    Log::Info("Initializing Local and I/O APIC...");
    APIC::Initialize();
    Log::Write("OK");

    Log::Info("Initializing SMP...");
    SMP::Initialize();
    Log::Write("OK");

    Memory::LateInitializeVirtualMemory();
}

void InitMultiboot2(uintptr_t _mbInfo) {
    InitCore();

    multiboot2_info_header_t* mbInfo = (multiboot2_info_header_t*)Memory::GetIOMapping(_mbInfo);

    multiboot2_tag_t* tag = reinterpret_cast<multiboot2_tag_t*>(mbInfo->tags);

    char* cmdLine = nullptr;
    multiboot2_module_t* modules[128];
    unsigned bootModuleCount = 0;

    while (tag->type && reinterpret_cast<uintptr_t>(tag) < reinterpret_cast<uintptr_t>(mbInfo) + mbInfo->totalSize) {
        switch (tag->type) {
        case Mboot2CmdLine: {
            multiboot2_cmdline_t* mbCmdLine = reinterpret_cast<multiboot2_cmdline_t*>(tag);

            cmdLine = mbCmdLine->string;
            break;
        }
        case Mboot2Modules: {
            if (bootModuleCount >= 128) {
                Log::Warning("Exceeded maximum amount of boot modules!");
                break;
            }

            multiboot2_module_t* mod = reinterpret_cast<multiboot2_module_t*>(tag);

            modules[bootModuleCount++] = mod;
            break;
        }
        case Mboot2MemoryInfo: {
            multiboot2_memory_info_t* mbMemInfo = reinterpret_cast<multiboot2_memory_info_t*>(tag);
            Log::Info("Bootloader reports %d MB of memory", (mbMemInfo->memoryLower + mbMemInfo->memoryUpper) / 1024);
            break;
        }
        case Mboot2MemoryMap: {
            multiboot2_memory_map_t* mbMemMap = reinterpret_cast<multiboot2_memory_map_t*>(tag);

            multiboot2_mmap_entry_t* currentEntry = mbMemMap->entries;

            mem_info.totalMemory = 0;
            while (reinterpret_cast<uintptr_t>(currentEntry) < reinterpret_cast<uintptr_t>(mbMemMap) + mbMemMap->size) {
                switch (currentEntry->type) {
                case 1: // Available
                    if (debugLevelHAL >= DebugLevelVerbose) {
                        Log::Info("Memory region [%x-%x] available", currentEntry->base,
                                  currentEntry->base + currentEntry->length);
                    }
                    Memory::MarkMemoryRegionFree(currentEntry->base, currentEntry->length);
                    mem_info.totalMemory += currentEntry->length;
                    break;
                default: // Not available
                    if (debugLevelHAL >= DebugLevelVerbose) {
                        Log::Info("Memory region [%x-%x] reserved", currentEntry->base,
                                  currentEntry->base + currentEntry->length);
                    }
                    break;
                }
                currentEntry =
                    reinterpret_cast<multiboot2_mmap_entry_t*>((uintptr_t)currentEntry + mbMemMap->entrySize);
            }
            Memory::usedPhysicalBlocks = 0;
            break;
        }
        case Mboot2FramebufferInfo: {
            multiboot2_framebuffer_info_t* mbFbInfo = reinterpret_cast<multiboot2_framebuffer_info_t*>(tag);

            videoMode.address = reinterpret_cast<void*>(Memory::KernelAllocate4KPages(
                (mbFbInfo->framebufferPitch * mbFbInfo->framebufferHeight + (PAGE_SIZE_4K - 1)) / PAGE_SIZE_4K));
            Memory::KernelMapVirtualMemory4K(
                mbFbInfo->framebufferAddr, (uintptr_t)videoMode.address,
                ((mbFbInfo->framebufferPitch * mbFbInfo->framebufferHeight + (PAGE_SIZE_4K - 1)) / PAGE_SIZE_4K),
                PAGE_PAT_WRITE_COMBINING | PAGE_WRITABLE | PAGE_PRESENT);

            videoMode.width = mbFbInfo->framebufferWidth;
            videoMode.height = mbFbInfo->framebufferHeight;
            videoMode.pitch = mbFbInfo->framebufferPitch;
            videoMode.bpp = mbFbInfo->framebufferBpp;

            videoMode.physicalAddress = mbFbInfo->framebufferAddr;

            videoMode.type = mbFbInfo->type;
            break;
        }
        case Mboot2ACPI1RSDP: {
            auto rsdp = &(reinterpret_cast<multiboot2_acpi1_rsdp_t*>(tag)->rsdp);
            ACPI::SetRSDP(new acpi_xsdp_t{*rsdp});

            Log::Debug(debugLevelHAL, DebugLevelVerbose, "Found MB2 ACPI 1 RSDP at %x", rsdp);
            break;
        }
        case Mboot2ACPI2RSDP: {
            auto rsdp = &(reinterpret_cast<multiboot2_acpi2_rsdp_t*>(tag)->rsdp);
            ACPI::SetRSDP(new acpi_xsdp_t{*rsdp});

            Log::Debug(debugLevelHAL, DebugLevelVerbose, "Found MB2 ACPI 2 RSDP at %x, revision: %d", rsdp,
                       rsdp->revision);
            break;
        }
        default: {
            if (debugLevelHAL >= DebugLevelVerbose) {
                Log::Info("Ignoring boot tag %d", tag->type);
            }
            break;
        }
        }

        tag = tag->next(); // Get next tag
    }

    Memory::MarkMemoryRegionUsed(0,
                                 (uintptr_t)&_end - KERNEL_VIRTUAL_BASE); // Make sure kernel memory is marked as used

    if (cmdLine) {
        char* savePtr = nullptr;
        cmdLine = strtok_r((char*)cmdLine, " ", &savePtr);

        while (cmdLine) {
            if (strcmp(cmdLine, "debug") == 0)
                debugMode = true;
            else if (strcmp(cmdLine, "nosmp") == 0)
                disableSMP = true;
            else if (strcmp(cmdLine, "kcon") == 0)
                useKCon = true;
            cmdLine = strtok_r(NULL, " ", &savePtr);
        }
    }

    // Manage Multiboot Modules
    if (debugLevelHAL >= DebugLevelNormal)
        Log::Info("Multiboot Module Count: %d", bootModuleCount);

    for (unsigned i = 0; i < bootModuleCount; i++) {
        multiboot2_module_t& mod = *modules[i];

        if (debugLevelHAL >= DebugLevelNormal) {
            Log::Info("    Multiboot Module %d [Start: %x, End: %x, Cmdline: %s]", i, mod.moduleStart, mod.moduleEnd,
                      mod.string);
        }

        Memory::MarkMemoryRegionUsed(mod.moduleStart, mod.moduleEnd - mod.moduleStart);
        bootModules[i] = {
            .base = Memory::GetIOMapping(mod.moduleStart),
            .size = mod.moduleEnd - mod.moduleStart,
        };
    }

    asm("sti");

    InitVideo();
    InitExtra();
}

void InitStivale2(uintptr_t st2Info) {
    InitCore();

    uintptr_t tagPhys = ((stivale2_info_header_t*)Memory::GetIOMapping(st2Info))->tags;
    char* cmdLine = nullptr;

    while (tagPhys) {
        tagPhys = Memory::GetIOMapping(tagPhys);

        stivale2_tag_t* tag = reinterpret_cast<stivale2_tag_t*>(tagPhys);
        Log::Debug(debugLevelHAL, DebugLevelVerbose, "[HAL] [stivale2] Found tag with ID: %x", tag->id);

        switch (tag->id) {
        case Stivale2TagCmdLine: {
            stivale2_tag_cmdline_t* cmdLineTag = reinterpret_cast<stivale2_tag_cmdline_t*>(tagPhys);

            cmdLine = reinterpret_cast<char*>(Memory::GetIOMapping(cmdLineTag->cmdLine));
            break;
        }
        case Stivale2TagMemoryMap: {
            stivale2_tag_memory_map_t* mmTag = reinterpret_cast<stivale2_tag_memory_map_t*>(tagPhys);

            if (mmTag->entryCount >
                (PAGE_SIZE_4K * 2) / sizeof(stivale2_tag_memory_map_t)) { // We only mapped two pages so check to make
                                                                          // sure we are within bounds
                mmTag->entryCount = (PAGE_SIZE_4K * 2) / sizeof(stivale2_tag_memory_map_t);
            }
            for (unsigned i = 0; i < mmTag->entryCount; i++) {
                stivale2_memory_map_entry_t& entry = mmTag->entries[i];
                switch (entry.type) {
                case Stivale2MMUsable:
                case Stivale2MMBootloaderReclaimable: // We don't do any allocations whilst parsing bootloader tags
                    Log::Debug(debugLevelHAL, DebugLevelVerbose, "Memory region [%x-%x] available", entry.base,
                               entry.base + entry.length);

                    Memory::MarkMemoryRegionFree(entry.base, entry.length);
                    mem_info.totalMemory += entry.length;
                    break;
                case Stivale2MMReserved:
                case Stivale2MMACPIReclaimable:
                case Stivale2MMACPINVS:
                case Stivale2MMBadMemory:
                case Stivale2MMKernelOrModule:
                    Log::Debug(debugLevelHAL, DebugLevelVerbose, "Memory region [%x-%x] claimed", entry.base,
                               entry.base + entry.length);
                    break;
                }
            }

            Memory::usedPhysicalBlocks = 0;
            break;
        }
        case Stivale2TagFramebufferInfo: {
            stivale2_tag_framebuffer_info_t* fbTag = reinterpret_cast<stivale2_tag_framebuffer_info_t*>(tagPhys);

            videoMode.address = reinterpret_cast<void*>(
                Memory::KernelAllocate4KPages((fbTag->fbPitch * fbTag->fbHeight + (PAGE_SIZE_4K - 1)) / PAGE_SIZE_4K));
            Memory::KernelMapVirtualMemory4K(fbTag->fbAddress, (uintptr_t)videoMode.address,
                                             ((fbTag->fbPitch * fbTag->fbHeight + (PAGE_SIZE_4K - 1)) / PAGE_SIZE_4K),
                                             PAGE_PAT_WRITE_COMBINING | PAGE_WRITABLE | PAGE_PRESENT);

            videoMode.width = fbTag->fbWidth;
            videoMode.height = fbTag->fbHeight;
            videoMode.pitch = fbTag->fbPitch;
            videoMode.bpp = fbTag->fbBpp;

            videoMode.physicalAddress = fbTag->fbAddress;

            if (fbTag->memoryModel == 1) { // RGB
                videoMode.type = VideoModeRGB;
            }
            break;
        }
        case Stivale2TagModules: {
            stivale2_tag_modules_t* modulesTag = reinterpret_cast<stivale2_tag_modules_t*>(tagPhys);

            for (unsigned i = 0; i < modulesTag->moduleCount && bootModuleCount < 128; i++) {
                stivale2_module_t& mod = modulesTag->modules[i];

                bootModules[bootModuleCount] = {.base = (uintptr_t)Memory::KernelAllocate4KPages(
                                                    (mod.end - mod.begin + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K),
                                                .size = mod.end - mod.begin};
                Memory::KernelMapVirtualMemory4K(mod.begin, bootModules[bootModuleCount].base,
                                                 (mod.end - mod.begin + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K);

                bootModuleCount++;
            }
            break;
        }
        case Stivale2TagACPIRSDP: {
            stivale2_tag_rsdp_t* rsdpTag = reinterpret_cast<stivale2_tag_rsdp_t*>(tag);

            ACPI::SetRSDP(new acpi_xsdp_t(*reinterpret_cast<acpi_xsdp_t*>(Memory::GetIOMapping(rsdpTag->rsdp))));
            break;
        }
        default:
            break;
        }

        tagPhys = tag->nextTag;
    }

    if (cmdLine) {
        char* savePtr = nullptr;
        cmdLine = strtok_r((char*)cmdLine, " ", &savePtr);

        while (cmdLine) {
            if (strcmp(cmdLine, "debug") == 0)
                debugMode = true;
            else if (strcmp(cmdLine, "nosmp") == 0)
                disableSMP = true;
            else if (strcmp(cmdLine, "kcon") == 0)
                useKCon = true;
            else if (strcmp(cmdLine, "runtests") == 0)
                runTests = true;
            cmdLine = strtok_r(NULL, " ", &savePtr);
        }
    }

    asm("sti");

    InitVideo();
    InitExtra();
}
} // namespace HAL

extern "C" [[noreturn]] void kmain();

extern "C" [[noreturn]] void kinit_multiboot2(uintptr_t mbInfo) {
    HAL::InitMultiboot2(mbInfo);

    kmain();
}

extern "C" [[noreturn]] void kinit_stivale2(uintptr_t st2Info) {
    HAL::InitStivale2(st2Info);

    kmain();
}