#include <types.h>
#include <logging.h>
#include <string.h>
#include <hal.h>
#include <video.h>
#include <videoconsole.h>
#include <liballoc.h>
#include <timer.h>
#include <math.h>
#include <initrd.h>
#include <mouse.h>
#include <keyboard.h>
#include <pci.h>
#include <panic.h>
#include <scheduler.h>
#include <syscalls.h>
#include <8254x.h>
#include <nvme.h>
#include <ahci.h>
#include <ata.h>
#include <xhci.h>
#include <devicemanager.h>
#include <gui.h>
#include <tar.h>
#include <sharedmem.h>

uint8_t* progressBuffer;
video_mode_t videoMode;

extern "C"
void IdleProcess(){
	asm("sti");

	Video::DrawBitmapImage(videoMode.width/2, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	Log::Info("Loading Init Process...");
	fs_node_t* initrd = fs::FindDir(fs::GetRoot(), "initrd");
	fs_node_t* initFsNode = fs::FindDir(initrd,"init.lef");
	if(!initFsNode){
		const char* panicReasons[]{
			"Failed to load init task (init.lef)!"
		};
		KernelPanic(panicReasons,1);
	}

	void* initElf = (void*)kmalloc(initFsNode->size);
	fs::Read(initFsNode, 0, initFsNode->size, (uint8_t*)initElf);
	asm("cli");

	process_t* initProc = Scheduler::CreateELFProcess(initElf);
	strcpy(initProc->workingDir, "/initrd");

	Log::Write("OK");

	for(;;){ // Use the idle task to clean up the windows of dead processes
		unsigned oldUptime = Timer::GetSystemUptime();
		while(Timer::GetSystemUptime() < oldUptime + 1) {
			Scheduler::Yield();
			asm("hlt");
		}

		if(!GetDesktop()) continue;

		desktop_t* desktop = GetDesktop();

		for(int i = 0; i < desktop->windows->windowCount; i++){
			handle_t handle = desktop->windows->windows[i];
			Window* win = (Window*)Scheduler::FindHandle(handle);
			if(!Scheduler::FindProcessByPID(win->info.ownerPID)){
				acquireLock(&desktop->lock);

				while(desktop->windows->dirty == 2);
				memcpy(&desktop->windows->windows[i], &desktop->windows->windows[i + 1], (desktop->windows->windowCount - i - 1));
				desktop->windows->windowCount--;
				desktop->windows->dirty = 1;

				releaseLock(&desktop->lock);
				
				Memory::DestroySharedMemory(win->info.primaryBufferKey);
				Memory::DestroySharedMemory(win->info.secondaryBufferKey);
			}
		}
	}
}

extern "C"
void kmain(multiboot_info_t* mb_info){
	HAL::Init(*mb_info);

	videoMode = Video::GetVideoMode();

	Log::SetVideoConsole(NULL);

	Memory::InitializeSharedMemory();

	fs::Initialize();
	Log::EnableBuffer();

	Video::DrawRect(0, 0, videoMode.width, videoMode.height, 0, 0, 0);

	Log::Info("Video Resolution: %dx%dx%d", videoMode.width, videoMode.height, videoMode.bpp);

	if(videoMode.height < 600)
		Log::Warning("Small Resolution, it is recommended to use a higher resoulution if possible.");
	if(videoMode.bpp != 32)
		Log::Warning("Unsupported Colour Depth expect issues.");

	Log::Info("RAM: %d MB", (mb_info->memoryHi + mb_info->memoryLo) / 1024);
	
	Log::Info("Initializing Ramdisk...");
	//Initrd::Initialize(initrd_start,initrd_end - initrd_start); // Initialize Initrd
	fs::tar::TarVolume* tar = new fs::tar::TarVolume(HAL::bootModules[0].base, HAL::bootModules[0].size, "initrd");
	fs::volumes->add_back(tar);
	tar = new fs::tar::TarVolume(HAL::bootModules[0].base, HAL::bootModules[0].size, "lib");
	fs::volumes->add_back(tar);
	Log::Write("OK");

	
	Log::Info("Filesystem Root:");

	fs_dirent_t dirent;
	fs_node_t* root = fs::GetRoot();
	int i = 0;
	while((fs::ReadDir(root,&dirent,i++))){ // Read until no more dirents
		fs_node_t* node = fs::FindDir(root, dirent.name);
		if(!node) continue;
		Log::Write("    ");
		if(node->flags & FS_NODE_DIRECTORY){
			Log::Write(dirent.name, 0, 255, 0);
		} else {
			Log::Write(dirent.name);
		}
		Log::Write("\n");
	}

	fs_node_t* initrd = fs::FindDir(fs::GetRoot(), "initrd");

	fs_node_t* splashFile;
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

	Video::DrawString("Copyright 2018-2020 JJ Roberts-White", 2, videoMode.height - 10, 255, 255, 255);

	Video::DrawBitmapImage(videoMode.width/2 - 24*4, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	DeviceManager::Init();
	
	Log::Info("Initializing HID...");

	Mouse::Install();
	Keyboard::Install();

	Log::Info("OK");
	
	Video::DrawBitmapImage(videoMode.width/2 - 24*3, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	Log::Info("Registering Syscall Handler...");

	InitializeSyscalls();

	Log::Write("OK");

	Video::DrawBitmapImage(videoMode.width/2 - 24*2, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	NVMe::Initialize();
	Intel8254x::Initialize();
	USB::XHCI::Initialize();
	ATA::Init();
	AHCI::Init();

	Video::DrawBitmapImage(videoMode.width/2 - 24*1, videoMode.height/2 + 292/2 + 48, 24, 24, progressBuffer);

	Log::Info("Initializing Task Scheduler...");
	Scheduler::Initialize();
	for(;;);
}
