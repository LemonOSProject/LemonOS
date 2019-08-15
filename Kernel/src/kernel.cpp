#include <types.h>
#include <logging.h>
#include <string.h>
#include <hal.h>
#include <video.h>
#include <videoconsole.h>
#include <liballoc.h>
#include <timer.h>
#include <scheduler.h>
#include <math.h>
#include <initrd.h>
#include <syscalls.h>
#include <gui.h>
#include <mouse.h>
#include <keyboard.h>
#include <pci.h>
#include <panic.h>

void* initElf;
extern "C"
void IdleProcess(){
	Log::Info("Loading init Process...");
	Log::SetVideoConsole(NULL);
	fs_node_t* initFsNode = fs::FindDir(Initrd::GetRoot(),"init.lef");
	if(!initFsNode){
		char* panicReasons[]{
			"Failed to load init task (init.lef)!"
		};
		KernelPanic(panicReasons,1);
	}
		
	initElf = (void*)kmalloc(initFsNode->size);
	fs::Read(initFsNode, 0, initFsNode->size, (uint8_t*)initElf);
	Scheduler::LoadELF(initElf);
	Log::Write("OK");
	for(;;){ 
		asm("hlt");
	}
}

extern "C"
void kmain(multiboot_info_t* mb_info){
	HAL::Init(*mb_info);

	video_mode_t videoMode = Video::GetVideoMode();

	VideoConsole con = VideoConsole(0,0,videoMode.width,videoMode.height);

	//Log::SetVideoConsole(&con);
	Log::SetVideoConsole(NULL);

	Video::DrawRect(0, 0, videoMode.width, videoMode.height, 0, 0, 0);

	char* stringBuffer = (char*)kmalloc(16);
	Log::Info("Video Resolution:");
	Log::Info(videoMode.width, false);
	Log::Write("x");
	Log::Write(itoa(videoMode.height, stringBuffer, 10));
	Log::Write("x");
	Log::Write(itoa(videoMode.bpp, stringBuffer, 10));
	kfree(stringBuffer);

	if(videoMode.height < 600)
		Log::Warning("Small Resolution, it is recommended to use a higher resoulution if possible.");
	if(videoMode.bpp != 32)
		Log::Warning("Unsupported Colour Depth expect issues.");

	stringBuffer = (char*)kmalloc(16);
	Log::Info("RAM: ");
	Log::Write(itoa((mb_info->memoryHi + mb_info->memoryLo) / 1024, stringBuffer, 10));
	Log::Write("MB");

	Log::Info("Initializing System Timer...");
	Timer::Initialize(1200);
	Log::Write("OK");

	Log::Info("Multiboot Module Count: ");
	Log::Info(HAL::multibootInfo.modsCount, false);

	Log::Info("Initializing Ramdisk...");
	multiboot_module_t initrdModule = *((multiboot_module_t*)HAL::multibootInfo.modsAddr); // Get initrd as a multiboot module
	Initrd::Initialize(initrdModule.mod_start,initrdModule.mod_end - initrdModule.mod_start); // Initialize Initrd
	Log::Write("OK");

	fs_node_t* splashFile;
	if(splashFile = fs::FindDir(Initrd::GetRoot(),"splash.bmp")){
		uint32_t size = splashFile->size;
		uint8_t* buffer = (uint8_t*)kmalloc(size);
		if(fs::Read(splashFile, 0, size, buffer))
			Video::DrawBitmapImage(videoMode.width/2 - 484/2, videoMode.height/2 - 292/2, 484, 292, buffer);
	}

	Video::DrawString("Copyright 2018-2019 JJ Roberts-White", 2, videoMode.height - 10, 255, 255, 255);

	long uptimeSeconds = Timer::GetSystemUptime();
	while((uptimeSeconds - Timer::GetSystemUptime()) < 2);

	Log::Info("Ramdisk Contents:\n");
	fs_dirent_t* dirent;
	fs_node_t* root = Initrd::GetRoot();
	int i = 0;
	while((dirent = fs::ReadDir(root,i++))){ // Read until no more dirents
		fs_node_t* node = fs::FindDir(root, dirent->name);
		if(!node) continue;
		Log::Write("    ");
		if(node->flags & FS_NODE_DIRECTORY){
			Log::Write(dirent->name, 0, 255, 0);
		} else {
			Log::Write(dirent->name);
		}
		Log::Write("\n");
	}

	Log::Info("Initializing HID...");

	Mouse::Install();
	Keyboard::Install();

	Log::Info("OK");

	Log::Info("Registering Syscall Handler...");

	InitializeSyscalls();

	Log::Write("OK");

	Log::Info("Initializing Task Scheduler...");
	
	Scheduler::Initialize();
	
	//Log::Write("OK");
}