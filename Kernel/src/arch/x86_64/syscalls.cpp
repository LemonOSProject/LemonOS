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
#include <timer.h>
#include <pty.h>

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
#define SYS_CHMOD 16
#define SYS_CREATE_DESKTOP 17
#define SYS_STAT 18
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
#define SYS_UPTIME 30
#define SYS_GET_VIDEO_MODE 31
#define SYS_UNAME 32
#define SYS_READDIR 33
#define SYS_SET_FS_BASE 34
#define SYS_MMAP 35
#define SYS_GRANT_PTY 36
#define SYS_GET_CWD 37
#define SYS_WAIT_PID 38
#define SYS_NANO_SLEEP 39
#define SYS_PREAD 40
#define SYS_PWRITE 41

#define NUM_SYSCALLS 42

#define EXEC_CHILD 1

typedef int(*syscall_t)(regs64_t*);

int SysExit(regs64_t* r){
	int64_t code = r->rbx;

	Log::Info("Process %d exiting with code %d", Scheduler::GetCurrentProcess()->pid, code);

	Scheduler::EndProcess(Scheduler::GetCurrentProcess());
	return 0;
}

int SysExec(regs64_t* r){
	char* filepath = (char*)kmalloc(strlen((char*)r->rbx) + 1);
	strcpy(filepath, (char*)r->rbx);
	int argc = r->rcx;
	char** argv = (char**)r->rdx;
	uint64_t flags = r->rsi;
	uint64_t* pid = (uint64_t*)r->rdi;

	*pid = 0;
	

	fs_node_t* current_node = fs::ResolvePath(filepath, Scheduler::GetCurrentProcess()->workingDir);

	if(!current_node){
		return 1;
	}

	Log::Info("Executing: %s", current_node->name);

	uint8_t* buffer = (uint8_t*)kmalloc(current_node->size);
	uint64_t a = current_node->read(current_node, 0, current_node->size, buffer);

	char** kernelArgv = (char**)kmalloc(argc * sizeof(char*));
	for(int i = 0; i < argc; i++){
		kernelArgv[i] = (char*)kmalloc(strlen(argv[i] + 1));
		strcpy(kernelArgv[i], argv[i]);
	}

	process_t* proc = Scheduler::CreateELFProcess((void*)buffer, argc, kernelArgv);

	for(int i = 0; i < argc; i++){
		kfree(kernelArgv[i]);
	}
	
	kfree(kernelArgv);
	kfree(buffer);

	if(!proc) return 1;

	if(flags & EXEC_CHILD){
		Scheduler::GetCurrentProcess()->children.add_back(proc);

		proc->fileDescriptors.replace_at(0, Scheduler::GetCurrentProcess()->fileDescriptors.get_at(0));
		proc->fileDescriptors.replace_at(1, Scheduler::GetCurrentProcess()->fileDescriptors.get_at(1));
		proc->fileDescriptors.replace_at(2, Scheduler::GetCurrentProcess()->fileDescriptors.get_at(2));
	}

	strncpy(proc->workingDir, Scheduler::GetCurrentProcess()->workingDir, PATH_MAX);

	*pid = proc->pid;

	return 0;
}

int SysRead(regs64_t* r){
	fs_fd_t* handle = Scheduler::GetCurrentProcess()->fileDescriptors.get_at(r->rbx);
	if(!handle){ 
		Log::Warning("read failed! file descriptor: %d", r->rbx);
		return 1; 
	}
	uint8_t* buffer = (uint8_t*)r->rcx;
	if(!buffer) { return 1; }
	uint64_t count = r->rdx;
	int ret = fs::Read(handle, count, buffer);
	*(int*)r->rsi = ret;
	return 0;
}

int SysWrite(regs64_t* r){
	if(r->rbx > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
		*((int*)r->rsi) = -1; // Return -1
		return -1;
	}
	fs_fd_t* handle = Scheduler::GetCurrentProcess()->fileDescriptors[r->rbx];
	if(!handle){
		Log::Warning("Invalid File Descriptor: %d", r->rbx);
		return 2;
	}

	if(!(r->rcx && r->rdx)) return 1;

	int ret = fs::Write(handle, r->rdx, (uint8_t*)r->rcx);

	if(r->rsi){
		*((int*)r->rsi) = ret;
	}

	return 0;
}

int SysOpen(regs64_t* r){
	char* filepath = (char*)kmalloc(strlen((char*)r->rbx) + 1);
	strcpy(filepath, (char*)r->rbx);
	fs_node_t* root = fs::GetRoot();
	fs_node_t* current_node = root;
	Log::Info("Opening: %s", filepath);
	uint32_t fd;
	if(strcmp(filepath,"/") == 0){
		fd = Scheduler::GetCurrentProcess()->fileDescriptors.get_length();
		Scheduler::GetCurrentProcess()->fileDescriptors.add_back(fs::Open(root, 0));
		*((uint32_t*)r->rcx) = fd;
		return 0;
	}

	fs_node_t* node = fs::ResolvePath(filepath, Scheduler::GetCurrentProcess()->workingDir);

	if(!node){
		Log::Warning("Failed to open file!");
		*((uint32_t*)r->rcx) = 0;
		return 1;
	}

	fd = Scheduler::GetCurrentProcess()->fileDescriptors.get_length();
	Scheduler::GetCurrentProcess()->fileDescriptors.add_back(fs::Open(node, 0));
	fs::Open(node, 0);

	*((uint32_t*)r->rcx) = fd;
	return 0;
}

int SysClose(regs64_t* r){
	int fd = r->rbx;
	fs_fd_t* handle;
	if(handle = Scheduler::GetCurrentProcess()->fileDescriptors[fd]){
		fs::Close(handle);
	}
	Scheduler::GetCurrentProcess()->fileDescriptors.replace_at(fd, NULL);
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
	if(r->rbx){
		if(((char*)r->rbx)[0] != '/') {
			strcpy(Scheduler::GetCurrentProcess()->workingDir + strlen(Scheduler::GetCurrentProcess()->workingDir), (char*)r->rbx);
		} else {
			strncpy(Scheduler::GetCurrentProcess()->workingDir, (char*)r->rbx, PATH_MAX);
		}
	} else Log::Warning("chdir: Invalid path string");
	return 0;
}

int SysTime(regs64_t* r){
	return 0;
}

int SysMapFB(regs64_t *r){
	video_mode_t vMode = Video::GetVideoMode();

	uintptr_t fbVirt = (uintptr_t)Memory::Allocate4KPages(vMode.height*vMode.pitch/PAGE_SIZE_4K + 2, Scheduler::currentProcess->addressSpace);
	Memory::MapVirtualMemory4K(HAL::multibootInfo.framebufferAddr,fbVirt,vMode.height*vMode.pitch/PAGE_SIZE_4K + 2,Scheduler::currentProcess->addressSpace);

	fb_info_t fbInfo;
	fbInfo.width = vMode.width;
	fbInfo.height = vMode.height;
	fbInfo.bpp = vMode.bpp;
	fbInfo.pitch = vMode.pitch;

	Log::Info("Mapping Framebuffer to:");
	Log::Info(fbVirt);

	*((uintptr_t*)r->rbx) = fbVirt;
	*((fb_info_t*)r->rcx) = fbInfo;

	Log::Info(r->rbx);
	Log::Info(r->rcx);
	Log::Info(*((uint64_t*)r->rbx));

	return 0;
}

int SysAlloc(regs64_t* r){
	uint64_t pageCount = r->rbx;
	uintptr_t* addressPointer = (uintptr_t*)r->rcx;

	uintptr_t address = (uintptr_t)Memory::Allocate4KPages(pageCount, Scheduler::currentProcess->addressSpace);
	for(int i = 0; i < pageCount; i++){
		Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),address + i * PAGE_SIZE_4K,1,Scheduler::currentProcess->addressSpace);
		memset((void*)(address + i * PAGE_SIZE_4K), 0, PAGE_SIZE_4K);
	}

	*addressPointer = address;
}

int SysChmod(regs64_t* r){
	return 0;
}

int SysStat(regs64_t* r){
	stat_t* stat = (stat_t*)r->rbx;
	int fd = r->rcx;
	int* ret = (int*)r->rdx;

	fs_node_t* node = Scheduler::GetCurrentProcess()->fileDescriptors.get_at(fd)->node;
	if(!node){
		Log::Warning("sys_stat: Invalid File Descriptor, %d", fd);
		*ret = 1;
		return 1;
	}

	stat->st_dev = 0;
	stat->st_ino = node->inode;
	
	if(node->flags & FS_NODE_DIRECTORY) stat->st_mode = S_IFDIR;
	if(node->flags & FS_NODE_FILE) stat->st_mode = S_IFREG;
	if(node->flags & FS_NODE_BLKDEVICE) stat->st_mode = S_IFBLK;
	if(node->flags & FS_NODE_CHARDEVICE) stat->st_mode = S_IFCHR;
	if(node->flags & FS_NODE_SYMLINK) stat->st_mode = S_IFLNK;

	stat->st_nlink = 0;
	stat->st_uid = node->uid;
	stat->st_gid = 0;
	stat->st_rdev = 0;
	stat->st_size = node->size;
	stat->st_blksize = 0;
	stat->st_blocks = 0;

	*ret = 0;

	return 0;
}

int SysLSeek(regs64_t* r){
	if(!(r->rsi)){
		Log::Warning("sys_lseek: Invalid Return Address");
		return 1;
	}

	int64_t* ret = (int64_t*)r->rsi;
	int fd = r->rbx;

	if(fd >= Scheduler::GetCurrentProcess()->fileDescriptors.get_length() || !Scheduler::GetCurrentProcess()->fileDescriptors[fd]){
		Log::Warning("sys_lseek: Invalid File Descriptor, %d", fd);
		*ret = -1;
		return 1;
	}

	switch(r->rdx){
	case 0: // SEEK_SET
		*ret = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->pos = r->rcx;
		return 0;
		break;
	case 1: // SEEK_CUR
		*ret = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->pos;
		return 0;
		break;
	case 2: // SEEK_END
		*ret = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->pos = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->node->size;
		return 0;
		break;
	default:
		Log::Info("Invalid seek: %s, mode: %d", Scheduler::GetCurrentProcess()->fileDescriptors[fd]->node->name, r->rdx);
		return 3; // Invalid seek mode
		break;
	}
	return 0;
}

int SysGetPID(regs64_t* r){
	uint64_t* pid = (uint64_t*)r->rbx;

	*pid = Scheduler::GetCurrentProcess()->pid;
	
	return 0;
}

int SysMount(regs64_t* r){
	return 0;
}

int SysCreateDesktop(regs64_t* r){
	desktop_t* desktop = (desktop_t*)kmalloc(sizeof(desktop_t));

	desktop->windows = new List<window_t*>();
	desktop->pid = Scheduler::GetCurrentProcess()->pid;

	SetDesktop(desktop);

	return 0;
}

int SysCreateWindow(regs64_t* r){
	win_info_t* info = (win_info_t*)r->rbx;
	window_t* win = (window_t*)kmalloc(sizeof(window_t));
	info->handle = Scheduler::RegisterHandle(win);
	info->ownerPID = Scheduler::GetCurrentProcess()->pid;
	win->info = *info;
	win->desktop = GetDesktop();

	surface_t surface;
	//win->buffer = (uint8_t*)kmalloc(info->width * info->height * 4);
	win->primaryBuffer = (uint8_t*)Memory::KernelAllocate4KPages((info->width * info->height * 4) / PAGE_SIZE_4K + 1);
	if(r->rcx){
		*((uint64_t*)r->rcx) = (uint64_t)Memory::Allocate4KPages((info->width * info->height * 4) / PAGE_SIZE_4K + 1, Scheduler::GetCurrentProcess()->addressSpace);
	}
	
	for(int i = 0; i < (info->width * info->height * 4) / PAGE_SIZE_4K + 1; i++){
		uint64_t phys = Memory::AllocatePhysicalMemoryBlock();
		Memory::KernelMapVirtualMemory4K(phys, (uintptr_t)win->primaryBuffer + i * PAGE_SIZE_4K, 1);
		if(r->rcx) Memory::MapVirtualMemory4K(phys, (*((uintptr_t*)r->rcx)) + i * PAGE_SIZE_4K, 1, Scheduler::GetCurrentProcess()->addressSpace);
	}

	win->secondaryBuffer = (uint8_t*)Memory::KernelAllocate4KPages((info->width * info->height * 4) / PAGE_SIZE_4K + 1);
	if(r->rdx){
		*((uint64_t*)r->rdx) = (uint64_t)Memory::Allocate4KPages((info->width * info->height * 4) / PAGE_SIZE_4K + 1, Scheduler::GetCurrentProcess()->addressSpace);
	}
	
	for(int i = 0; i < (info->width * info->height * 4) / PAGE_SIZE_4K + 1; i++){
		uint64_t phys = Memory::AllocatePhysicalMemoryBlock();
		Memory::KernelMapVirtualMemory4K(phys, (uintptr_t)win->secondaryBuffer + i * PAGE_SIZE_4K, 1);
		if(r->rdx) Memory::MapVirtualMemory4K(phys, (*((uintptr_t*)r->rdx)) + i * PAGE_SIZE_4K, 1, Scheduler::GetCurrentProcess()->addressSpace);
	}

	win->surface = surface;
	win->surface.buffer = win->primaryBuffer;


	GetDesktop()->windows->add_back(win);

	message_t createMessage;
	createMessage.msg = 0xBEEF; // Desktop Event
	createMessage.data = 2; // Subevent - Window Created
	createMessage.recieverPID = GetDesktop()->pid;
	createMessage.senderPID = 0;
	Scheduler::SendMessage(createMessage);

	return 0;
}

int SysDestroyWindow(regs64_t* r){
	handle_t handle = (handle_t)r->rbx;

	window_t* win;
	if(win = (window_t*)Scheduler::FindHandle(handle)){
		for(int i = 0; i < GetDesktop()->windows->get_length(); i++){
			if(GetDesktop()->windows->get_at(i) == win){
				GetDesktop()->windows->remove_at(i);
				message_t destroyMessage;
				destroyMessage.msg = 0xBEEF; // Desktop Event
				destroyMessage.data = 1; // Subevent - Window Destroyed
				destroyMessage.recieverPID = GetDesktop()->pid;
				destroyMessage.senderPID = 0;
				Scheduler::SendMessage(destroyMessage);
				break;
			}
		}
	} else return 2;

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
	int buffer = r->rcx;
	win_info_t* info = (win_info_t*)r->rdx;
	Window* window = (Window*)Scheduler::FindHandle(handle);
	if(!window) return 2;

	if(info){ // Update Window Info
		win_info_t oldInfo = window->info;
		window->info = *info;

		if(oldInfo.width != info->width || oldInfo.height != info->height){
			// TODO: Ensure that enough memory is allocated in the case of a resized window
			Log::Warning("sys_update_window: Window has been resized");
		}

		message_t updateMessage;
		updateMessage.msg = 0xBEEF; // Desktop Event
		updateMessage.data = 2; // Subevent - Window Created
		updateMessage.recieverPID = GetDesktop()->pid;
		updateMessage.senderPID = 0;
		Scheduler::SendMessage(updateMessage); // Notify the window manager that windows have been updated

		return 0;
	} else { // Swap Window Buffers
		switch(buffer){
		case 1:
			window->surface.buffer = window->primaryBuffer;
			break;
		case 2:
			window->surface.buffer = window->secondaryBuffer;
			break;
		default:
			window->surface.buffer = (window->surface.buffer == window->primaryBuffer) ? window->secondaryBuffer : window->primaryBuffer;
			break;
		}
	}

	return 1;
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

	if(rowSize <= 0) return 0;

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
	uint64_t* queueSize = (uint64_t*)r->rcx;

	*queueSize = Scheduler::GetCurrentProcess()->messageQueue.get_length();
	*msg = Scheduler::RecieveMessage(Scheduler::GetCurrentProcess());

	return 0;
}

int SysUptime(regs64_t* r){
	uint64_t* seconds = (uint64_t*)r->rbx;
	uint64_t* milliseconds = (uint64_t*)r->rcx;
	if(seconds){
		*seconds = Timer::GetSystemUptime();
	}
	if(milliseconds){
		*milliseconds = ((double)Timer::GetTicks())/(Timer::GetFrequency()/1000.0);
	}
}

int SysDebug(regs64_t* r){
	asm("cli");
	Log::Info((char*)r->rbx);
	Log::Info(r->rcx);
	asm("sti");
	return 0;
}

int SysGetVideoMode(regs64_t* r){
	video_mode_t vMode = Video::GetVideoMode();
	fb_info_t fbInfo;
	fbInfo.width = vMode.width;
	fbInfo.height = vMode.height;
	fbInfo.bpp = vMode.bpp;
	fbInfo.pitch = vMode.pitch;

	*((fb_info_t*)r->rbx) = fbInfo;

	return 0;
}

namespace Lemon { extern char* versionString; };
int SysUName(regs64_t* r){
	char* str = (char*)r->rbx;
	strcpy(str, Lemon::versionString);

	return 0;
}

int SysReadDir(regs64_t* r){
	if(!(r->rbx && r->rcx)) return 1;

	unsigned int fd = r->rbx;

	if(fd > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
		if(r->rsi) *((uint64_t*)r->rsi) = 0;
		return 2;
	} 
	
	fs_dirent_t* direntPointer = (fs_dirent_t*)r->rcx;

	unsigned int count = r->rdx;

	if(!(Scheduler::GetCurrentProcess()->fileDescriptors[fd]->node->flags & FS_NODE_DIRECTORY)){
		if(r->rsi) *((uint64_t*)r->rsi) = 0;
		return 2;
	}

	fs_dirent_t* dirent = fs::ReadDir(Scheduler::GetCurrentProcess()->fileDescriptors[fd],count);

	if(!dirent) {
		if(r->rsi) *((uint64_t*)r->rsi) = 0;
		return 2;
	}

	direntPointer->inode = dirent->inode;
	direntPointer->type = dirent->type;
	strcpy(direntPointer->name,dirent->name);
	direntPointer->name[strlen(dirent->name)] = 0;
	if(r->rsi) *((uint64_t*)r->rsi) = 1;
	return 0;
}

int SysSetFsBase(regs64_t* r){
	asm("wrmsr" :: "a"(r->rbx & 0xFFFFFFFF) /*Value low*/, "b"((r->rbx >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000100) /*Set FS Base*/);
	return 0;
}

int SysMmap(regs64_t* r){
	uint64_t* address = (uint64_t*)r->rbx;
	size_t count = r->rcx;
	uintptr_t hint = r->rdx;

	uintptr_t _address;
	if(hint){
		if(Memory::CheckRegion(hint, count * PAGE_SIZE_4K, Scheduler::GetCurrentProcess()->addressSpace) /*Check availibilty of the requested map*/){
			_address = hint;
		} else {
			*address = 0;
			return 1;
		}
	} else _address = (uintptr_t)Memory::Allocate4KPages(count, Scheduler::GetCurrentProcess()->addressSpace);

	for(int i = 0; i < count; i++){
		Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), _address + i * PAGE_SIZE_4K, 1, Scheduler::currentProcess->addressSpace);
		memset((void*)(_address + i * PAGE_SIZE_4K), 0, PAGE_SIZE_4K);
	}

	*address = _address;

	return 0;
}

int SysGrantPTY(regs64_t* r){
	if(!r->rbx) return 1;

	PTY* pty = GrantPTY(Scheduler::GetCurrentProcess()->pid);

	process_t* currentProcess = Scheduler::GetCurrentProcess();

	*((int*)r->rbx) = currentProcess->fileDescriptors.get_length();
	
	currentProcess->fileDescriptors.replace_at(0, fs::Open(&pty->slaveFile)); // Stdin
	currentProcess->fileDescriptors.replace_at(1, fs::Open(&pty->slaveFile)); // Stdout
	currentProcess->fileDescriptors.replace_at(2, fs::Open(&pty->slaveFile)); // Stderr

	currentProcess->fileDescriptors.add_back(fs::Open(&pty->masterFile));

	return 0;
}

int SysGetCWD(regs64_t* r){
	char* buf = (char*)r->rbx;
	size_t sz = r->rcx;
	int* ret = (int*)r->rdx;

	char* workingDir = Scheduler::GetCurrentProcess()->workingDir;
	if(strlen(workingDir) > sz) {
		*ret = 1;
	} else {
		strcpy(buf, workingDir);
		*ret = 0;
	}
}

int SysWaitPID(regs64_t* r){
	uint64_t pid = r->rbx;

	while(Scheduler::FindProcessByPID(pid)) asm("hlt");

	return 0;
}

int SysNanoSleep(regs64_t* r){
	uint64_t nanoseconds = r->rbx;

	uint64_t seconds = Timer::GetSystemUptime();
	uint64_t ticks = seconds * Timer::GetFrequency() + Timer::GetTicks();
	uint64_t ticksEnd = ticks + (int)(nanoseconds / (1000000000.0 / Timer::GetFrequency()));

	while((Timer::GetSystemUptime() * Timer::GetFrequency() + Timer::GetTicks()) < ticksEnd) asm("pause");
}

int SysPRead(regs64_t* r){
	if(r->rbx > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
		*((int*)r->rsi) = -1; // Return -1
		return -1;
	}
	fs_node_t* node;
	if(Scheduler::GetCurrentProcess()->fileDescriptors.get_at(r->rbx) || !(node = Scheduler::GetCurrentProcess()->fileDescriptors.get_at(r->rbx)->node)){ 
		Log::Warning("sys_pread: Invalid file descriptor: %d", r->rbx);
		return 1; 
	}
	uint8_t* buffer = (uint8_t*)r->rcx;
	if(!buffer) { return 1; }
	uint64_t count = r->rdx;
	uint64_t off = r->rdi;
	int ret = node->read(node, off, count, buffer);
	*(int*)r->rsi = ret;
	return 0;
}

int SysPWrite(regs64_t* r){
	if(r->rbx > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
		*((int*)r->rsi) = -1; // Return -1
		return -1;
	}
	fs_node_t* node;
	if(Scheduler::GetCurrentProcess()->fileDescriptors.get_at(r->rbx) || !(node = Scheduler::GetCurrentProcess()->fileDescriptors.get_at(r->rbx)->node)){ 
		Log::Warning("sys_pwrite: Invalid file descriptor: %d", r->rbx);
		return 1; 
	}

	if(!(r->rcx)) {
		*((int*)r->rsi) = -1;
		return 1;
	}
	uint64_t off = r->rdi;

	int ret = fs::Write(node, off, r->rdx, (uint8_t*)r->rcx);

	if(r->rsi){
		*((int*)r->rsi) = ret;
	}

	return 0;
}

syscall_t syscalls[]{
	/*nullptr*/SysDebug,
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
	SysUptime,					// 30
	SysGetVideoMode,
	SysUName,
	SysReadDir,
	SysSetFsBase,
	SysMmap,					// 35
	SysGrantPTY,
	SysGetCWD,
	SysWaitPID,
	SysNanoSleep,
	SysPRead,					// 40
	SysPWrite,
};

namespace Scheduler{extern bool schedulerLock;};

void SyscallHandler(regs64_t* regs) {
	if (regs->rax >= NUM_SYSCALLS) // If syscall is non-existant then return
		return;
		
	asm("sti");

	int ret = syscalls[regs->rax](regs); // Call syscall
	regs->rax = ret;
}

void InitializeSyscalls() {
	IDT::RegisterInterruptHandler(0x69,SyscallHandler);
}