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

extern "C"
void IdleProcess(){
	Log::Info("Loading init Process...");
	Log::SetVideoConsole(NULL);
	void* initElf;
	fs_node_t* initFsNode = fs::FindDir(Initrd::GetRoot(),"init.lef");
	initElf = (void*)kmalloc(initFsNode->size);
	fs::Read(initFsNode, 0, initFsNode->size, (uint8_t*)initElf);
	Scheduler::LoadELF(initElf);
	Log::Write("OK");
	Log::SetVideoConsole(NULL);
	while(!GetDesktop());
	for(;;){ 
		memcpy((void*)HAL::videoMode.address, GetDesktop()->surface.buffer, HAL::videoMode.width*HAL::videoMode.height*4);
	}
}

extern "C"
void kmain(multiboot_info_t* mb_info){
	HAL::Init(*mb_info);

	video_mode_t videoMode = Video::GetVideoMode();

	uint8_t gray = 0;
	uint8_t op = 0;
	for(int i = 0; i < videoMode.height; i += 1){
		Video::DrawRect(0, i, videoMode.width, 1, gray, gray, gray);

		if(gray >= 255) op = 1;
		else if(gray <= 0) op = 0;

		if(op == 0) gray++;
		else gray--;
	}

	VideoConsole con = VideoConsole(0,0,videoMode.width,videoMode.height);

	Log::SetVideoConsole(&con);

	char* stringBuffer = (char*)kmalloc(16);
	Log::Info("Video Resolution:");
	Log::Info(videoMode.width, false);
	Log::Write("x");
	Log::Write(itoa(videoMode.height, stringBuffer, 10));
	Log::Write("x");
	Log::Write(itoa(videoMode.bpp, stringBuffer, 10));
	kfree(stringBuffer);

	con.Update();

	if(videoMode.height < 800)
		Log::Warning("Small Resolution, it is recommended to use a higher resoulution if possible.");
	if(videoMode.bpp != 32)
		Log::Warning("Unsupported Colour Depth expect issues.");

	stringBuffer = (char*)kmalloc(16);
	Log::Info("RAM: ");
	Log::Write(itoa((mb_info->memoryHi + mb_info->memoryLo) / 1024, stringBuffer, 10));
	Log::Write("MB");

	Log::Info("Initializing System Timer...");
	Timer::Initialize(1000);
	Log::Write("OK");

	Log::Info("Multiboot Module Count: ");
	Log::Info(HAL::multibootInfo.modsCount, false);

	Log::Info("Initializing Ramdisk...");
	multiboot_module_t initrdModule = *((multiboot_module_t*)HAL::multibootInfo.modsAddr); // Get initrd as a multiboot module
	Initrd::Initialize(initrdModule.mod_start,initrdModule.mod_end - initrdModule.mod_start); // Initialize Initrd
	Log::Write("OK");

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