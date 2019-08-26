#include <syscalls.h>

#include <video.h>
#include <memory.h>
#include <scheduler.h>
#include <hal.h>
#include <string.h>
#include <initrd.h>
#include <filesystem.h>
#include <idt.h>
#include <logging.h>
#include <gui.h>
#include <timer.h>
#include <pty.h>
#include <lemon.h>
#include <systeminfo.h>
#include <cpuid.h>

#define NUM_SYSCALLS 38

#define SYS_EXIT 1
#define SYS_EXEC 2
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_SLEEP 7
#define SYS_CREATE 8
#define SYS_LINK 9
#define SYS_UNLINK 10
#define SYS_CHDIR 12
#define SYS_TIME 13
#define SYS_MSG_SEND 14
#define SYS_MSG_RECIEVE 15
#define SYS_UNAME 16
#define SYS_MEMALLOC 17
#define SYS_STAT 18
#define SYS_LSEEK 19
#define SYS_GETPID 20
#define SYS_MOUNT 21
#define SYS_UMOUNT 22
#define SYS_CREATE_DESKTOP 23
#define SYS_CREATE_WINDOW 24
#define SYS_PAINT_DESKTOP 25
#define SYS_PAINT_WINDOW 26
#define SYS_DESKTOP_GET_WINDOW 27
#define SYS_DESKTOP_GET_WINDOW_COUNT 28
#define SYS_COPY_WINDOW_SURFACE 29
#define SYS_READDIR 30
#define SYS_UPTIME 31
#define SYS_GETVIDEOMODE 32
#define SYS_DESTROY_WINDOW 33
#define SYS_GRANTPTY 34
#define SYS_FORKEXEC 35
#define SYS_GET_SYSINFO 36

typedef int(*syscall_t)(regs32_t*);

int SysExit(regs32_t* r){
	Scheduler::EndProcess(Scheduler::GetCurrentProcess());
	return 0;
}

int SysExec(regs32_t* r){
	char* filepath = (char*)r->ebx;
	
	fs_node_t* root = Initrd::GetRoot();
	fs_node_t* current_node = root;
	char* file = strtok(filepath,"/");
	while(file != NULL){ // Iterate through the directories to find the file
		fs_node_t* node = current_node->findDir(current_node,file);
		if(!node) return 1;
		if(node->flags & FS_NODE_DIRECTORY){
			current_node = node;
			file = strtok(NULL, "/");
			continue;
		}
		current_node = node;
		break;
	}

	uint8_t* buffer = (uint8_t*)kmalloc(current_node->size);
	uint32_t a = current_node->read(current_node, 0, current_node->size, buffer);
	Scheduler::LoadELF((void*)buffer);
	kfree(buffer);
	return 0;
}

int SysRead(regs32_t* r){
	fs_node_t* node = Scheduler::GetCurrentProcess()->fileDescriptors[r->ebx];
	if(!node){ Log::Warning("read failed! file descriptor: "); Log::Info(r->ebx, false); return 1; }
	uint8_t* buffer = (uint8_t*)r->ecx;
	if(!buffer) return 1;
	uint32_t count = r->edx;
	int ret = node->read(node, 0, count, buffer);
	*(int*)r->esi = ret;
	return 0;
}

int SysWrite(regs32_t* r){
	fs_node_t* node = Scheduler::GetCurrentProcess()->fileDescriptors[r->ebx];
	if(!node){
		Log::Warning("Invalid File Descriptor");
		Log::Warning(r->ebx);
		return 2;
	}

	if(!(r->ecx && r->edx)) return 1;

	uint8_t* buffer = (uint8_t*)kmalloc(r->edx);
	memcpy(buffer, (uint8_t*)r->ecx, r->edx);

	int ret = fs::Write(node, 0, r->edx, buffer);

	if(r->esi){
		*((int*)r->esi) = ret;
	}

	return 0;
}

int SysOpen(regs32_t* r){
	char* filepath = (char*)r->ebx;
	fs_node_t* root = Initrd::GetRoot();
	fs_node_t* current_node = root;
	Log::Info("Opening: ");
	Log::Write(filepath);
	uint32_t fd;
	if(strcmp(filepath,"/") == 0){

		fd = Scheduler::GetCurrentProcess()->fileDescriptors.get_length();
		Scheduler::GetCurrentProcess()->fileDescriptors.add_back(root);
		*((uint32_t*)r->ecx) = fd;
		return 0;
	}

	char* file = strtok(filepath,"/");
	while(file != NULL){
		fs_node_t* node = current_node->findDir(current_node,file);
		if(!node) return 1;
		if(node->flags & FS_NODE_DIRECTORY){
			current_node = node;
			file = strtok(NULL, "/");
			continue;
		}
		fd = Scheduler::GetCurrentProcess()->fileDescriptors.get_length();
		Scheduler::GetCurrentProcess()->fileDescriptors.add_back(node);
		break;
	}

	*((uint32_t*)r->ecx) = fd;
	return 0;
}

int SysClose(regs32_t* r){
	int fd = r->ebx;
	fs_node_t* node = Scheduler::GetCurrentProcess()->fileDescriptors[fd];
	if(!node){
		Log::Warning("Invalid File Descriptor");
		Log::Warning(r->ebx);
		return 2;
	}
	fs::Close(node);
	return 0;
}

int SysSleep(regs32_t* r){

}

int SysCreate(regs32_t* r){
	return 0;
}

int SysLink(regs32_t* r){
	return 0;
}

int SysUnlink(regs32_t* r){
	return 0;
}

int SysExec2(regs32_t* r){
	return SysExec(r);
}

int SysChdir(regs32_t* r){

}

int SysTime(regs32_t* r){

}

// SendMessage(message_t* msg) - Sends an IPC message to a process
int SysSendMessage(regs32_t* r){
	uint32_t pid = r->ebx;
	uint32_t id = r->ecx;
	uint32_t data = r->edx;
	uint32_t data2 = r->esi;

	message_t message;
	message.senderPID = Scheduler::GetCurrentProcess()->pid;
	message.recieverPID = pid;
	message.id = id;
	message.data = data;
	message.data2 = data2;

	return Scheduler::SendMessage(message); // Send the message
}

// RecieveMessage(message_t* msg) - Grabs next message on queue and copies it to msg
int SysReceiveMessage(regs32_t* r){
	if(!(r->ebx && r->ecx)) return 1; // Was given null pointers

	message_t* msg = (message_t*)r->ebx;
	uint32_t* queueSize = (uint32_t*)r->ecx;

	*queueSize = Scheduler::GetCurrentProcess()->messageQueue.get_length();
	*msg = Scheduler::RecieveMessage(Scheduler::GetCurrentProcess());

	return 0;
}

int SysUname(regs32_t* r){
	if(!r->ebx) return 1;
	else strcpy((char*)r->ebx, Lemon::versionString);
}

int SysMemalloc(regs32_t* r){
	if(!(r->ebx && r->ecx)) return 1;
	asm("cli");
	uint32_t virt = Memory::AllocateVirtualPages(r->ebx, Scheduler::GetCurrentProcess()->pageDirectory);

	for(uint32_t i = 0; i < r->ebx; i++){
		uint32_t phys = Memory::AllocatePhysicalMemoryBlock();
		Memory::MapVirtualPages(phys,virt + i * PAGE_SIZE, 1, Scheduler::GetCurrentProcess()->pageDirectory);
	}

	uint32_t* addr = (uint32_t*)r->ecx;

	*addr = virt;
	asm("sti");
	return 0;
}

int SysStat(regs32_t* r){

}

int SysLSeek(regs32_t* r){
	if(!(r->esi)){
		return 1;
	}

	uint32_t* ret = (uint32_t*)r->esi;
	int fd = r->ebx;

	if(fd >= Scheduler::GetCurrentProcess()->fileDescriptors.get_length() || !Scheduler::GetCurrentProcess()->fileDescriptors[fd]){
		return 1;
	}

	switch(r->edx){
	case 0: // SEEK_SET
		return 2; // Not implemented
		break;
	case 1: // SEEK_CUR
		return 2; // Not implemented
		break;
	case 2: // SEEK_END
		*ret = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->size;
		return 0;
		break;
	default:
		return 3; // Invalid seek mode
		break;
	}
}

int SysGetPid(regs32_t* r){
	uint32_t* pid_ptr = (uint32_t*)r->ebx;
	*pid_ptr = Scheduler::GetCurrentProcess()->pid;
	return 0;
}

int SysMount(regs32_t* r){

}

int SysUmount(regs32_t* r){

}

int SysCreateDesktop(regs32_t* r){
	if(!(r->ebx && r->ecx)) return 1;
	handle_t* handle_ptr = (handle_t*)r->ebx;
	surface_t* surface_ptr = (surface_t*)r->ecx;
	handle_t handle = (handle_t)kmalloc(sizeof(desktop_t));
	Scheduler::RegisterHandle(handle);
	desktop_t* desktop = (desktop_t*)handle;
	SetDesktop(desktop);
	desktop->windows = new List<window_t*>();
	desktop->surface.buffer = (uint8_t*)kmalloc(HAL::videoMode.pitch*HAL::videoMode.height);
	desktop->surface.height = HAL::videoMode.height;
	desktop->surface.width = HAL::videoMode.width;
	surface_ptr->width = HAL::videoMode.width;
	surface_ptr->height = HAL::videoMode.height;
	surface_ptr->depth = 32;

	*handle_ptr = handle;

	return 0;
}

int SysCreateWindow(regs32_t* r){
	handle_t* handlePtr = (handle_t*)r->ebx;
	if(!r->ecx) return 1;
	if(!r->ebx) return 1;
	win_info_t winfo = *(win_info_t*)r->ecx;
	handle_t handle = (handle_t)kmalloc(sizeof(window_t));
	winfo.handle = handle;
	if(handlePtr) *handlePtr = handle;
	Scheduler::RegisterHandle(handle);
	window_t* win = (window_t*)handle;
	win->info = winfo;
	strcpy(win->info.title, winfo.title);
	win->info.ownerPID = Scheduler::GetCurrentProcess()->pid;
	//win->buffer = (uint8_t*)kmalloc(win->info.width*win->info.height*4+4);
	surface_t surface;
	bool needsPadding = (winfo.width * 4) % 0x10;
	int horizontalSizePadded = winfo.width * 4 + (0x10 - ((winfo.width * 4) % 0x10));
	surface.buffer = (uint8_t*)kmalloc(horizontalSizePadded * winfo.height);
	//surface.linePadding = (0x10 - ((winfo.width * 4) % 0x10));
	win->surface = surface;
	GetDesktop()->windows->add_back((window_t*)handle);
	return 0;
}

int SysPaintDesktop(regs32_t* r){
	if(!(r->ebx && r->ecx)) return 1;
	handle_t handle = (handle_t)r->ebx;
	surface_t* surface = (surface_t*)r->ecx;

	if(!(Scheduler::FindHandle(handle).handle)) return 1;
	desktop_t* desktop = (desktop_t*)handle;

	memcpy_optimized(/*desktop->surface.buffer*/(void*)HAL::videoMode.address,surface->buffer,desktop->surface.width*desktop->surface.height*4);
	return 0;
}

// Updates Window Surface (handle_t window, surface_t* windowSurface)
int SysPaintWindow(regs32_t* r){
	if(!(r->ebx && r->ecx)) return 1;
	handle_t handle = (handle_t)r->ebx;
	surface_t* surface = (surface_t*)r->ecx;

	if(!(Scheduler::FindHandle(handle).handle)) return 1;
	window_t* window = (window_t*)handle;

	memcpy_optimized(window->surface.buffer,surface->buffer,(window->info.width * 4 /*+ window->surface.linePadding*/)*window->info.height);
	return 0;
}

// Retrieves window from desktop (uint32_t windowIndex, win_info_t* windowStruct)
int SysDesktopGetWindow(regs32_t* r){
	if(!(r->ecx)){
		return 1;
	}

	win_info_t* infoStruct = (win_info_t*)r->ecx;

	window_t* window = (*(GetDesktop()->windows))[r->ebx];

	if(!window) return 2;

	*infoStruct = window->info;
	//strcpy(infoStruct->title, window->info.title);
	return 0;
}

// Get window count (uint32_t* windowCount)
int SysDesktopGetWindowCount(regs32_t* r){
	if(!r->ebx) return 1;
	*((uint32_t*)r->ebx) = GetDesktop()->windows->get_length();

	return 0;
}

// Copy from window surface to another surface (surface_t dest, handle_t win, vector2i_t offset)
int SysCopyWindowSurface(regs32_t* r){
	if(!(r->ebx && r->ecx)) return 1;
	vector2i_t offset;
	if(!r->edx) offset = {0,0};
	else offset = *((vector2i_t*)r->edx);

	handle_t handle = (handle_t)r->ecx;
	if(!(Scheduler::FindHandle(handle).handle)) return 2;
	
	window_t* window = (window_t*)handle;

	rect_t windowRegion;
	if(!r->edx) windowRegion = {{0,0},{window->info.width,window->info.height}};
	else windowRegion = *((rect_t*)r->esi);

	surface_t* dest = (surface_t*)r->ebx;

	int rowSize = ((dest->width - offset.x) > windowRegion.size.x) ? windowRegion.size.x : (dest->width - offset.x);

	for(int i = windowRegion.pos.y; i < windowRegion.size.y && i < dest->height - offset.y; i++){
		uint8_t* bufferAddress = dest->buffer + (i + offset.y) * (dest->width * 4 /*+ dest->linePadding*/)+ offset.x * 4;
		//if(!((uint32_t)bufferAddress % 0x10))
			memcpy_optimized(bufferAddress, window->surface.buffer + i * (window->info.width * 4 /*+ window->surface.linePadding*/) + windowRegion.pos.x * 4, rowSize * 4);
		//else
		//	memcpy(bufferAddress, window->surface.buffer + i * window->info.width * 4, rowSize*4);
	}

	return 0;
}

int SysReadDir(regs32_t* r){
	Log::Info("reading dir");
	if(!(r->ebx && r->ecx)) return 1;

	unsigned int fd = r->ebx;

	if(fd > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()) return 2;
	
	fs_dirent_t* direntPointer = (fs_dirent_t*)r->ecx;

	unsigned int count = r->edx;

	Log::Info("reading dir2");

	fs_dirent_t* dirent = fs::ReadDir(Scheduler::GetCurrentProcess()->fileDescriptors[fd],count);

	if(!dirent) {
		if(r->esi) *((uint32_t*)r->esi) = 0;
		return 2;
	}

	direntPointer->inode = dirent->inode;
	direntPointer->type = dirent->type;
	strcpy(direntPointer->name,dirent->name);
	direntPointer->name[strlen(dirent->name)] = 0;
	if(r->esi) *((uint32_t*)r->esi) = 1;

	Log::Info("reading dir3");
	return 0;
}

int SysUptime(regs32_t* r){
	uint32_t* seconds = (uint32_t*)r->ebx;
	uint32_t* milliseconds = (uint32_t*)r->ecx;
	if(seconds){
		*seconds = Timer::GetSystemUptime();
	}
	if(milliseconds){
		*milliseconds = Timer::GetTicks()/(Timer::GetFrequency()/1000);
	}
}

int SysGetVideoMode(regs32_t* r){
	if(!r->ebx) return 1;
	video_mode_t* v = (video_mode_t*)r->ebx;
	*v = HAL::videoMode;
	return 0;
}

int SysDestroyWindow(regs32_t* r){
	if(!(r->ebx)) return 1;
	handle_t handle = (handle_t)r->ebx;

	if(!(Scheduler::FindHandle(handle).handle)) return 1;
	window_t* win = (window_t*)handle;

	desktop_t* desktop = GetDesktop();

	for(int i = 0; i < GetDesktop()->windows->get_length(); i++){
		if((*(desktop->windows))[i] == win)
			desktop->windows->remove_at(i);
	}

	kfree(win);

	return 0;
}

int SysGrantPty(regs32_t* r){
	if(!r->ebx) return 1;

	PTY* pty = GrantPTY(Scheduler::GetCurrentProcess()->pid);

	process_t* currentProcess = Scheduler::GetCurrentProcess();

	*((uint32_t*)r->ebx) = currentProcess->fileDescriptors.get_length();

	currentProcess->fileDescriptors.add_back(&pty->masterFile);

	currentProcess->fileDescriptors.replace_at(0, &pty->slaveFile); // Stdin
	currentProcess->fileDescriptors.replace_at(1, &pty->slaveFile); // Stdout
	currentProcess->fileDescriptors.replace_at(2, &pty->slaveFile); // Stderr

	return 0;
}

int SysForkExec(regs32_t* r){
	char* filepath = (char*)r->ebx;
	
	fs_node_t* root = Initrd::GetRoot();
	fs_node_t* current_node = root;
	char* file = strtok(filepath,"/");
	while(file != NULL){ // Iterate through the directories to find the file
		fs_node_t* node = current_node->findDir(current_node,file);
		if(!node) return 1;
		if(node->flags & FS_NODE_DIRECTORY){
			current_node = node;
			file = strtok(NULL, "/");
			continue;
		}
		current_node = node;
		break;
	}

	uint8_t* buffer = (uint8_t*)kmalloc(current_node->size);
	uint32_t a = current_node->read(current_node, 0, current_node->size, buffer);
	uint64_t pid = Scheduler::LoadELF((void*)buffer);
	process_t* proc = Scheduler::FindProcessByPID(pid);
	process_t* currentProc = Scheduler::GetCurrentProcess();
	proc->fileDescriptors.clear();
	for(int i = 0; i < currentProc->fileDescriptors.get_length(); i++){
		proc->fileDescriptors.add_back(currentProc->fileDescriptors[i]);
	}
	kfree(buffer);
	return 0;
}

int SysGetSysinfo(regs32_t* r){
	if(!r->ebx) return 1;

	sys_info_t sysInfo;
	sysInfo.memSize = HAL::multibootInfo.memoryHi + HAL::multibootInfo.memoryLo;
	sysInfo.usedMemory = Memory::usedPhysicalBlocks;

	strcpy(sysInfo.versionString, Lemon::versionString);
	strcpy(sysInfo.cpuVendor, CPU::GetCPUInfo().vendor_string);

	memcpy((void*)r->ebx, &sysInfo, sizeof(sys_info_t));
	return 0;
}

int SysDebug(regs32_t* r){
	Log::Info((char*)r->ebx);
	Log::Info(r->ecx);
	return 0;
}

syscall_t syscalls[]{
	nullptr,
	SysExit,					// 1
	SysExec,
	SysRead,
	SysWrite,
	SysOpen,					// 5
	SysClose,
	SysSleep,
	SysCreate,
	SysLink,
	SysUnlink,					// 10
	SysExec,
	SysChdir,
	SysTime,
	SysSendMessage,
	SysReceiveMessage,			// 15
	SysUname,
	SysMemalloc,
	SysStat,
	SysLSeek,
	SysGetPid,					// 20
	SysMount,
	SysUmount,
	SysCreateDesktop,
	SysCreateWindow,
	SysPaintDesktop,			// 25
	SysPaintWindow,
	SysDesktopGetWindow,
	SysDesktopGetWindowCount,
	SysCopyWindowSurface,
	SysReadDir,
	SysUptime,
	SysGetVideoMode,
	SysDestroyWindow,
	SysGrantPty,
	SysForkExec,
	SysGetSysinfo,
	SysDebug
};

namespace Scheduler{extern bool schedulerLock;};

void SyscallHandler(regs32_t* regs) {
	if (regs->eax >= NUM_SYSCALLS) // If syscall is non-existant then return
		return;
		
	Scheduler::schedulerLock = true;
	int ret = syscalls[regs->eax](regs); // Call syscall
	if(ret) {
		/*Log::Warning("Syscall was probably not successful!");
		Log::Info(regs->eax, false);
		Log::Info("Exit Code: ");
		Log::Info(ret, false);*/
	}
	regs->eax = ret;
	Scheduler::schedulerLock = false;
}

void InitializeSyscalls() {
	IDT::RegisterInterruptHandler(0x69,SyscallHandler);
}