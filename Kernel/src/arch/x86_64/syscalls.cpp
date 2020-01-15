#include <syscalls.h>

#include <idt.h>
#include <scheduler.h>
#include <logging.h>
#include <initrd.h>
#include <video.h>
#include <hal.h>
#include <fb.h>
#include <physicalallocator.h>
#include <gui.h>

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
#define SYS_MAP_FB 14
#define SYS_ALLOC 15
#define SYS_CREATE_DESKTOP 17

#define SYS_LSEEK 19
#define SYS_GETPID 20
#define SYS_MOUNT 21
#define SYS_CREATE_WINDOW 22
#define SYS_DESTROY_WINDOW 23
#define SYS_DESKTOP_GET_WINDOW 24
#define SYS_DESKTOP_GET_WINDOW_COUNT 25
#define SYS_UPDATE_WINDOW 26
#define SYS_RENDER_WINDOW 27
#define SYS_SEND_MESSAGE 28
#define SYS_RECEIVE_MESSAGE 29

typedef int(*syscall_t)(regs64_t*);

int SysExit(regs64_t* r){
	Scheduler::EndProcess(Scheduler::GetCurrentProcess());
	return 0;
}

int SysExec(regs64_t* r){
	char* filepath = (char*)r->rbx;
	
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
	uint64_t a = current_node->read(current_node, 0, current_node->size, buffer);
	Scheduler::LoadELF((void*)buffer);
	kfree(buffer);
	return 0;
}

int SysRead(regs64_t* r){
	fs_node_t* node = Scheduler::GetCurrentProcess()->fileDescriptors[r->rbx];
	if(!node){ Log::Warning("read failed! file descriptor: "); Log::Info(r->rbx, false); return 1; }
	uint8_t* buffer = (uint8_t*)r->rcx;
	if(!buffer) { return 1; }
	uint64_t count = r->rdx;
	int ret = node->read(node, 0, count, buffer);
	*(int*)r->rsi = ret;
	return 0;
}

int SysWrite(regs64_t* r){
	return 0;
}

int SysOpen(regs64_t* r){
	char* filepath = (char*)r->rbx;
	fs_node_t* root = Initrd::GetRoot();
	fs_node_t* current_node = root;
	Log::Info("Opening: ");
	Log::Write(filepath);
	uint32_t fd;
	if(strcmp(filepath,"/") == 0){

		fd = Scheduler::GetCurrentProcess()->fileDescriptors.get_length();
		Scheduler::GetCurrentProcess()->fileDescriptors.add_back(root);
		*((uint32_t*)r->rcx) = fd;
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

	*((uint32_t*)r->rcx) = fd;
	return 0;
}

int SysClose(regs64_t* r){
	return 0;
}

int SysSleep(regs64_t* r){
	return 0;
}

int SysCreate(regs64_t* r){
	return 0;
}

int SysLink(regs64_t* r){
	return 0;
}

int SysUnlink(regs64_t* r){
	return 0;
}

int SysChdir(regs64_t* r){
	return 0;
}

int SysTime(regs64_t* r){
	return 0;
}

int SysMapFB(regs64_t *r){
	video_mode_t vMode = Video::GetVideoMode();

	uintptr_t fbVirt = (uintptr_t)Memory::Allocate4KPages(vMode.height*vMode.pitch/PAGE_SIZE_4K + 1, Scheduler::currentProcess->addressSpace);
	Memory::MapVirtualMemory4K(HAL::multibootInfo.framebufferAddr,fbVirt,vMode.height*vMode.pitch/PAGE_SIZE_4K + 1,Scheduler::currentProcess->addressSpace);

	fb_info_t fbInfo;
	fbInfo.width = vMode.width;
	fbInfo.height = vMode.height;
	fbInfo.bpp = vMode.bpp;
	fbInfo.pitch = vMode.pitch;

	*((uintptr_t*)r->rbx) = fbVirt;
	*((fb_info_t*)r->rcx) = fbInfo;

	return 0;
}

int SysAlloc(regs64_t* r){
	uint64_t pageCount = r->rbx;
	uintptr_t* addressPointer = (uintptr_t*)r->rcx;

	uintptr_t address = (uintptr_t)Memory::Allocate4KPages(pageCount, Scheduler::currentProcess->addressSpace);
	for(int i = 0; i < pageCount; i++){
		Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),address + i * PAGE_SIZE_4K,1,Scheduler::currentProcess->addressSpace);
	}

	*addressPointer = address;
}

int SysChmod(regs64_t* r){
	return 0;
}

int SysStat(regs64_t* r){
	return 0;
}

int SysLSeek(regs64_t* r){
	if(!(r->rsi)){
		return 1;
	}

	uint32_t* ret = (uint32_t*)r->rsi;
	int fd = r->rbx;

	if(fd >= Scheduler::GetCurrentProcess()->fileDescriptors.get_length() || !Scheduler::GetCurrentProcess()->fileDescriptors[fd]){
		return 1;
	}

	switch(r->rdx){
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
	return 0;
}

int SysGetPID(regs64_t* r){
	return 0;
}

int SysMount(regs64_t* r){
	return 0;
}

int SysCreateDesktop(regs64_t* r){
	desktop_t* desktop = (desktop_t*)kmalloc(sizeof(desktop_t));

	desktop->windows = new List<window_t*>();

	SetDesktop(desktop);

	return 0;
}

int SysCreateWindow(regs64_t* r){
	win_info_t* info = (win_info_t*)r->rbx;
	window_t* win = (window_t*)kmalloc(sizeof(window_t));
	info->handle = Scheduler::RegisterHandle(win);
	win->info = *info;
	win->desktop = GetDesktop();

	surface_t surface;
	surface.buffer = (uint8_t*)kmalloc(info->width * info->height * 4);

	win->surface = surface;

	GetDesktop()->windows->add_back(win);
	return 0;
}

int SysDestroyWindow(regs64_t* r){
	handle_t handle = (handle_t)r->rbx;
	return 0;
}

int SysDesktopGetWindow(regs64_t* r){
	win_info_t* winInfo = (win_info_t*)r->rbx;

	*winInfo = GetDesktop()->windows->get_at(r->rcx)->info;

	return 0;
}

int SysDesktopGetWindowCount(regs64_t* r){
	uint64_t* count = (uint64_t*)r->rbx;
	*count = GetDesktop()->windows->get_length();
	
	return 0;
}

int SysUpdateWindow(regs64_t* r){
	handle_t handle = (handle_t*)r->rbx;
	Window* window = (Window*)Scheduler::FindHandle(handle);
	surface_t* surface = (surface_t*)r->rcx;

	memcpy_optimized(window->surface.buffer,surface->buffer,(window->info.width * 4)*window->info.height);
	return 0;
}

// SysRenderWindow(surface_t* dest, handle_t win, vector2i_t* offset, rect_t* region)
int SysRenderWindow(regs64_t* r){
	if(!(r->rbx && r->rcx)) return 1; // We need at least the destination surface and the window handle
	vector2i_t offset;
	if(!r->rdx) offset = {0,0};
	else offset = *((vector2i_t*)r->rdx);

	handle_t handle = (handle_t)r->rcx;
	window_t* window;
	if(!(window = (window_t*)Scheduler::FindHandle(handle))) return 2;

	rect_t windowRegion;
	if(!r->rsi) windowRegion = {{0,0},{window->info.width,window->info.height}};
	else windowRegion = *((rect_t*)r->rsi);

	surface_t* dest = (surface_t*)r->rbx;

	int rowSize = ((dest->width - offset.x) > windowRegion.size.x) ? windowRegion.size.x : (dest->width - offset.x);

	for(int i = windowRegion.pos.y; i < windowRegion.size.y && i < dest->height - offset.y; i++){
		uint8_t* bufferAddress = dest->buffer + (i + offset.y) * (dest->width * 4) + offset.x * 4;
		memcpy_optimized(bufferAddress, window->surface.buffer + i * (window->info.width * 4) + windowRegion.pos.x * 4, rowSize * 4);
	}
	return 0;
}

// SendMessage(message_t* msg) - Sends an IPC message to a process
int SysSendMessage(regs64_t* r){
	uint64_t pid = r->rbx;
	uint64_t msg = r->rcx;
	uint64_t data = r->rdx;
	uint64_t data2 = r->rsi;

	message_t message;
	message.senderPID = Scheduler::GetCurrentProcess()->pid;
	message.recieverPID = pid;
	message.msg = msg;
	message.data = data;
	message.data2 = data2;

	return Scheduler::SendMessage(message); // Send the message
}

// RecieveMessage(message_t* msg) - Grabs next message on queue and copies it to msg
int SysReceiveMessage(regs64_t* r){
	if(!(r->rbx && r->rcx)) return 1; // Was given null pointers

	message_t* msg = (message_t*)r->rbx;
	uint32_t* queueSize = (uint32_t*)r->rcx;

	*queueSize = Scheduler::GetCurrentProcess()->messageQueue.get_length();
	*msg = Scheduler::RecieveMessage(Scheduler::GetCurrentProcess());

	return 0;
}

int SysDebug(regs64_t* r){
	Log::Info((char*)r->rbx);
	Log::Info(r->rcx);
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
	SysMapFB,
	SysAlloc,					// 15
	SysChmod,
	SysCreateDesktop,
	SysStat,
	SysLSeek,
	SysGetPID,					// 20
	SysMount,
	SysCreateWindow,
	SysDestroyWindow,
	SysDesktopGetWindow,
	SysDesktopGetWindowCount,	// 25
	SysUpdateWindow,
	SysRenderWindow,
	SysSendMessage,
	SysReceiveMessage,

	/*SysUname,
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
	SysGetSysinfo,*/
	SysDebug
};

namespace Scheduler{extern bool schedulerLock;};

void SyscallHandler(regs64_t* regs) {
	//if (regs->rax >= NUM_SYSCALLS) // If syscall is non-existant then return
	//	return;
		
	Scheduler::schedulerLock = true;
	int ret = syscalls[regs->rax](regs); // Call syscall
	regs->rax = ret;
	Scheduler::schedulerLock = false;
}

void InitializeSyscalls() {
	IDT::RegisterInterruptHandler(0x69,SyscallHandler);
}