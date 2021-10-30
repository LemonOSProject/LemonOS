#include <CPU.h>
#include <Fs/TAR.h>
#include <Fs/Tmp.h>
#include <Fs/VolumeManager.h>
#include <HAL.h>
#include <Lemon.h>
#include <Logging.h>
#include <MM/KMalloc.h>
#include <Math.h>
#include <Modules.h>
#include <Net/Net.h>
#include <Objects/Service.h>
#include <PCI.h>
#include <PS2.h>
#include <Panic.h>
#include <Scheduler.h>
#include <SharedMemory.h>
#include <Storage/AHCI.h>
#include <Storage/ATA.h>
#include <Storage/NVMe.h>
#include <String.h>
#include <Symbols.h>
#include <Syscalls.h>
#include <TTY/PTY.h>
#include <Timer.h>
#include <Types.h>
#include <USB/XHCI.h>
#include <Video/Video.h>

#include <Debug.h>

video_mode_t videoMode;

void IdleProcess() {
    Thread* th = Scheduler::GetCurrentThread();
    for (;;) {
        th->timeSlice = 0;
        asm volatile("pause");
    }
}

void KernelProcess() {
    NVMe::Initialize();
    USB::XHCIController::Initialize();
    ATA::Init();
    AHCI::Init();

    ServiceFS::Initialize();

    Network::InitializeConnections();

    if (FsNode* node = fs::ResolvePath("/initrd/modules.cfg")) {
        char* buffer = new char[node->size + 1];

        ssize_t read = fs::Read(node, 0, node->size, buffer);
        if (read > 0) {
            buffer[read] = 0; // Null-terminate the buffer

            char* save;
            char* path = strtok_r(buffer, "\n", &save);
            while (path) {
                if (strlen(path) > 0) {
                    ModuleManager::LoadModule(path); // modules.cfg should contain a list of paths to modules
                }

                path = strtok_r(nullptr, "\n", &save);
            }
        }

        delete[] buffer;
    }

    fs::VolumeManager::MountSystemVolume();

    // TODO: Move this to userspace
    fs::VolumeManager::RegisterVolume(new fs::LinkVolume("/system/etc", "etc"));
    fs::VolumeManager::RegisterVolume(new fs::LinkVolume("/system/bin", "bin"));

    if (FsNode* node = fs::ResolvePath("/system/lib")) {
        fs::VolumeManager::RegisterVolume(new fs::LinkVolume("/system/lib", "lib"));
    } else {
        FsNode* initrd = fs::ResolvePath("/initrd");
        assert(initrd);

        fs::VolumeManager::RegisterVolume(
            new fs::LinkVolume("/initrd", "lib")); // If /system/lib is not present is /initrd
    }

    if (HAL::runTests) {
        ModuleManager::LoadModule("/initrd/modules/testmodule.sys");
        Log::Warning("Finished running tests. Hanging.");
        for (;;)
            ;
    }

    PTMultiplexor::Initialize();

    Log::Info("Loading Init Process...");
    FsNode* initFsNode = nullptr;

    if (HAL::useKCon || !(initFsNode = fs::ResolvePath("/system/lemon/init.lef"))) { // Attempt to start fterm
        initFsNode = fs::ResolvePath("/initrd/fterm.lef");

        if (!initFsNode) { // Shit has really hit the fan and fterm is not on the ramdisk
            const char* panicReasons[]{"Failed to load either init task (init.lef) or fterm (fterm.lef)!"};
            KernelPanic(panicReasons, 1);
        }
    }

    Log::Write("OK");

    void* initElf = (void*)kmalloc(initFsNode->size);
    fs::Read(initFsNode, 0, initFsNode->size, (uint8_t*)initElf);

    auto initProc = Process::CreateELFProcess(initElf, Vector<String>("init"), Vector<String>("PATH=/initrd"),
                                              "/system/lemon/init.lef", nullptr);
    initProc->Start();

    for (;;) {
        acquireLock(&Scheduler::destroyedProcessesLock);
        for (auto it = Scheduler::destroyedProcesses->begin(); it != Scheduler::destroyedProcesses->end(); it++) {
            if (!(acquireTestLock(&(*it)->m_processLock))) {
                FancyRefPtr<Process> proc = *it;
                if (proc->addressSpace) { // Destroy the address space regardless
                    delete (proc)->addressSpace;
                    proc->addressSpace = nullptr;
                }

                Scheduler::destroyedProcesses->remove(it);
                releaseLock(&proc->m_processLock);
            }

            if (it == Scheduler::destroyedProcesses->end()) {
                break;
            }
        }
        releaseLock(&Scheduler::destroyedProcessesLock);

        Scheduler::GetCurrentThread()->Sleep(100000);
    }
}

typedef void (*ctor_t)(void);
extern ctor_t _ctors_start[0];
extern ctor_t _ctors_end[0];
extern "C" void _init();

void InitializeConstructors() {
    unsigned ctorCount = ((uint64_t)&_ctors_end - (uint64_t)&_ctors_start) / sizeof(void*);

    for (unsigned i = 0; i < ctorCount; i++) {
        _ctors_start[i]();
    }
}

extern "C" [[noreturn]] void kmain() {
    fs::VolumeManager::Initialize();
    DeviceManager::Initialize();
    Log::LateInitialize();

    InitializeConstructors(); // Call global constructors

    DeviceManager::RegisterFSVolume();
    Log::EnableBuffer();

    videoMode = Video::GetVideoMode();

    if (debugLevelMisc >= DebugLevelVerbose) {
        Log::Info("Video Resolution: %dx%dx%d", videoMode.width, videoMode.height, videoMode.bpp);
    }

    if (videoMode.height < 600)
        Log::Warning("Small Resolution, it is recommended to use a higher resoulution if possible.");
    if (videoMode.bpp != 32)
        Log::Warning("Unsupported Colour Depth expect issues.");

    Video::DrawRect(0, 0, videoMode.width, videoMode.height, 0, 0, 0);

    Log::Info("Used RAM: %d MB", Memory::usedPhysicalBlocks * 4096 / 1024 / 1024);

    assert(fs::GetRoot());

    Log::Info("Initializing Ramdisk...");

    fs::tar::TarVolume* tar = new fs::tar::TarVolume(HAL::bootModules[0].base, HAL::bootModules[0].size, "initrd");
    fs::VolumeManager::RegisterVolume(tar);

    Log::Write("OK");

    fs::VolumeManager::RegisterVolume(new fs::Temp::TempVolume("tmp")); // Create tmpfs instance

    FsNode* initrd = fs::FindDir(fs::GetRoot(), "initrd");
    FsNode* splashFile = nullptr;
    FsNode* symbolFile = nullptr;

    if (initrd) {
        if ((splashFile = fs::FindDir(initrd, "splash.bmp"))) {
            uint32_t size = splashFile->size;
            uint8_t* buffer = new uint8_t[size];

            if (fs::Read(splashFile, 0, size, buffer) > 0)
                Video::DrawBitmapImage(videoMode.width / 2 - 620 / 2, videoMode.height / 2 - 150 / 2, 621, 150, buffer);

            delete[] buffer;
        } else
            Log::Warning("Could not load splash image");

        if ((symbolFile = fs::FindDir(initrd, "kernel.map"))) {
            LoadSymbolsFromFile(symbolFile);
        } else {
            KernelPanic((const char*[]){"Failed to locate kernel.map!"}, 1);
        }
    } else {
        KernelPanic((const char*[]){"initrd not mounted!"}, 1);
    }

    Video::DrawString("Copyright 2018-2021 JJ Roberts-White", 2, videoMode.height - 10, 255, 255, 255);
    Video::DrawString(Lemon::versionString, 2, videoMode.height - 20, 255, 255, 255);

    Log::Info("Initializing HID...");

    PS2::Initialize();

    Log::Info("OK");

    Log::Info("Initializing Task Scheduler...");
    Scheduler::Initialize();
    for (;;)
        ;
}
