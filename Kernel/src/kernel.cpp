#include <types.h>
#include <logging.h>
#include <string.h>
#include <hal.h>
#include <video.h>
#include <liballoc.h>
#include <timer.h>
#include <math.h>
#include <mouse.h>
#include <keyboard.h>
#include <pci.h>
#include <panic.h>
#include <scheduler.h>
#include <syscalls.h>
#include <net/8254x.h>
#include <nvme.h>
#include <ahci.h>
#include <ata.h>
#include <xhci.h>
#include <devicemanager.h>
#include <gui.h>
#include <fs/tar.h>
#include <sharedmem.h>
#include <net/net.h>
#include <cpu.h>

uint8_t* progressBuffer = nullptr;
video_mode_t videoMode;

extern "C"
void IdleProcess(){
	for(;;) {
		asm("sti");
		Scheduler::Yield();
		asm("hlt");
	}
}

void KernelProcess(){
	if(progressBuffer)
		Video::DrawBitmapImage(videoMode.width/2 - 24*1, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	NVMe::Initialize();
	USB::XHCI::Initialize();
	ATA::Init();
	AHCI::Init();
	
	Network::InitializeDrivers();
	Network::InitializeConnections();

	asm("cli");

	if(progressBuffer)
		Video::DrawBitmapImage(videoMode.width/2, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	Log::Info("Loading Init Process...");
	FsNode* initFsNode = nullptr;

	if(HAL::useKCon || !(initFsNode = fs::ResolvePath("/system/lemon/init.lef"))){ // Attempt to start fterm
		initFsNode = fs::ResolvePath("/initrd/fterm.lef");

		if(!initFsNode){ // Shit has really hit the fan and fterm is not on the ramdisk
			const char* panicReasons[]{
				"Failed to load either init task (init.lef) or fterm (fterm.lef)!"
			};
			KernelPanic(panicReasons,1);
		}
	}

	void* initElf = (void*)kmalloc(initFsNode->size);
	fs::Read(initFsNode, 0, initFsNode->size, (uint8_t*)initElf);

	char* argv[] = {"init.lef"};
	process_t* initProc = Scheduler::CreateELFProcess(initElf, 1, argv);

	strcpy(initProc->workingDir, "/");
	strcpy(initProc->name, "Init");

	Log::Write("OK");

	//Scheduler::EndProcess(Scheduler::GetCurrentProcess());
	for(;;) {
		GetCPULocal()->currentThread->state = ThreadStateBlocked;
		Scheduler::Yield();
	}
}

typedef void (*ctor_t)(void);
extern ctor_t _ctors_start[0];
extern ctor_t _ctors_end[0];

void InitializeConstructors(){
	unsigned ctorCount = ((uint64_t)&_ctors_end - (uint64_t)&_ctors_start) / sizeof(void*);

	Log::Info("[Constructors] Start: %x, End: %x, Count: %d", _ctors_start, _ctors_end, ctorCount);

	for(unsigned i = 0; i < ctorCount; i++){
		_ctors_start[i]();
	}
}

extern "C"
[[noreturn]] void kmain(multiboot2_info_header_t* mb_info){
	HAL::Init(mb_info);

	fs::Initialize();
	
	InitializeConstructors(); // Call global constructors

	Log::LateInitialize();
    Log::EnableBuffer();

	DeviceManager::InitializeBasicDevices();

	videoMode = Video::GetVideoMode();

	Memory::InitializeSharedMemory();

	Log::Info("Video Resolution: %dx%dx%d", videoMode.width, videoMode.height, videoMode.bpp);

	if(videoMode.height < 600)
		Log::Warning("Small Resolution, it is recommended to use a higher resoulution if possible.");
	if(videoMode.bpp != 32)
		Log::Warning("Unsupported Colour Depth expect issues.");

	Video::DrawRect(0, 0, videoMode.width, videoMode.height, 0, 0, 0);

	//Log::Info("System RAM: %d MB", (HAL::multibootInfo.memoryHi + HAL::multibootInfo.memoryLo) / 1024);
	Log::Info("Reserved RAM: %d MB", Memory::usedPhysicalBlocks * 4096 / 1024 / 1024);
	
	Log::Info("Initializing Ramdisk...");
	
	fs::tar::TarVolume* tar = new fs::tar::TarVolume(HAL::bootModules[0].base, HAL::bootModules[0].size, "initrd");
	fs::volumes->add_back(tar);
	fs::volumes->add_back(new fs::LinkVolume(tar, "lib"));
	Log::Write("OK");

	FsNode* initrd = fs::FindDir(fs::GetRoot(), "initrd");
	FsNode* splashFile = nullptr;

	if(initrd){
		if((splashFile = fs::FindDir(initrd,"splash.bmp"))){
			uint32_t size = splashFile->size;
			uint8_t* buffer = (uint8_t*)kmalloc(size);
			if(fs::Read(splashFile, 0, size, buffer))
				Video::DrawBitmapImage(videoMode.width/2 - 484/2, videoMode.height/2 - 292/2, 484, 292, buffer);
		} else Log::Warning("Could not load splash image");

		if((splashFile = fs::FindDir(initrd,"pbar.bmp"))){
			uint32_t size = splashFile->size;
			progressBuffer = (uint8_t*)kmalloc(size);
			if(fs::Read(splashFile, 0, size, progressBuffer)){
				Video::DrawBitmapImage(videoMode.width/2 - 24*6, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);
				Video::DrawBitmapImage(videoMode.width/2 - 24*5, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);
			}
		} else Log::Warning("Could not load progress bar image");
	} else {
		Log::Warning("Failed to find initrd!");
	}

	Video::DrawString("Copyright 2018-2020 JJ Roberts-White", 2, videoMode.height - 10, 255, 255, 255);

	if(progressBuffer)
		Video::DrawBitmapImage(videoMode.width/2 - 24*4, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	Log::Info("Initializing HID...");

	Mouse::Install();
	Keyboard::Install();

	Log::Info("OK");
	
	if(progressBuffer)
		Video::DrawBitmapImage(videoMode.width/2 - 24*3, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	Log::Info("Registering Syscall Handler...");

	InitializeSyscalls();

	Log::Write("OK");

	if(progressBuffer)
		Video::DrawBitmapImage(videoMode.width/2 - 24*2, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	Log::Info("Initializing Task Scheduler...");
	Scheduler::Initialize();
	for(;;);
}
