#include <Audio/Audio.h>
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
#include <TTY/PTY.h>
#include <Timer.h>
#include <Types.h>
#include <USB/XHCI.h>
#include <Video/Video.h>
#include <hiraku.h>

#include <Debug.h>

video_mode_t videoMode;

void IdleProcess() {
    Thread* th = Thread::current();
    for (;;) {
        th->timeSlice = 0;
        asm volatile("pause");
    }
}

void syscall_init();

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

void KernelProcess() {
    fs::VolumeManager::Initialize();
    DeviceManager::Initialize();
    Log::LateInitialize();

    InitializeConstructors(); // Call global constructors

    DeviceManager::RegisterFSVolume();
    Log::EnableBuffer();

    videoMode = Video::GetVideoMode();

    Log::Info("Video Resolution: %dx%dx%d", videoMode.width, videoMode.height, videoMode.bpp);

    Log::Info("Used RAM: %d MB", Memory::usedPhysicalBlocks * 4 / 1024);

    Log::Info("Mounting initrd, tmpfs...");

    fs::tar::TarVolume* tar = new fs::tar::TarVolume(HAL::bootModules[0].base, HAL::bootModules[0].size, "initrd");
    fs::VolumeManager::RegisterVolume(tar);

    fs::VolumeManager::RegisterVolume(new fs::Temp::TempVolume("tmp")); // Create tmpfs instance

    Log::write("OK");

    LoadSymbols();

    syscall_init();

    Log::Info("Initializing HID...");
    PS2::Initialize();
    Log::Info("OK");

    NVMe::Initialize();
    USB::XHCIController::Initialize();
    ATA::Init();
    AHCI::Init();

    Network::InitializeConnections();
    Audio::InitializeSystem();

    FsNode* initrd = fs::ResolvePath("/initrd");
    assert(initrd);

    fs::VolumeManager::RegisterVolume(
        new fs::LinkVolume("/initrd", "lib")); // If /system/lib is not present is /initrd

    if (HAL::runTests) {
        ModuleManager::LoadModule("/initrd/modules/testmodule.sys");
        Log::Warning("Finished running tests. Hanging.");
        for (;;)
            ;
    }

    PTMultiplexor::Initialize();

    Log::Info("Loading Init Process...");
    FsNode* initNode = fs::ResolvePath("/initrd/init.lef");

    Log::write("OK");

    LoadHirakuSymbols();

    FancyRefPtr<File> file = ({
        auto r = fs::Open(initNode);
        if(r.HasError()) {
            KernelPanic("Error opening /initrd/init.lef");
        }

        r.Value();
    });


    auto initProc = ({
        auto r = Process::create_elf_process(file, Vector<String>("init"), Vector<String>("PATH=/initrd"),
                                              "/initrd/init.lef", nullptr);
        if(r.HasError()) {
            KernelPanic("Error executing /initrd/init.lef");
        }

        r.Value();
    });

    if(HAL::debugMode) {
        ScopedSpinLock lock{Log::logLock};

        delete Log::console;
        Log::SetVideoConsole(new VideoConsole(0, (videoMode.height / 3) * 2, videoMode.width, videoMode.height / 3));
    } else {
        ScopedSpinLock lock{Log::logLock};

        delete Log::console;
        Log::SetVideoConsole(NULL);
    }

    initProc->start();

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

        Thread::current()->sleep(100000);
    }
}

extern "C" [[noreturn]] void kmain() {
    fs::create_root_fs();
    Scheduler::Initialize();
    KernelPanic("Failed to create kernel process!");
}
