#include <syscalls.h>

#include <idt.h>
#include <scheduler.h>
#include <logging.h>
#include <initrd.h>
#include <video.h>
#include <hal.h>
#include <fb.h>
#include <physicalallocator.h>

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

#define SYS_LSEEK 19
#define SYS_READDIR 0
#define SYS_UNAME 0
#define SYS_UPTIME 0

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
	int ret;// = node->read(node, 0, count, buffer);
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

	uintptr_t fbVirt = (uintptr_t)Memory::KernelAllocate4KPages(vMode.height*vMode.pitch/PAGE_SIZE_4K + 1);//, Scheduler::currentProcess->addressSpace);
	Memory::KernelMapVirtualMemory4K(HAL::multibootInfo.framebufferAddr,fbVirt,vMode.height*vMode.pitch/PAGE_SIZE_4K + 1);//,Scheduler::currentProcess->addressSpace);

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


		Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),address,1,Scheduler::currentProcess->addressSpace);
		((uint8_t*)address)[0] = 0;

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
	SysStat,
	nullptr,
	SysLSeek,
	SysGetPID,
	
	/*SysSendMessage,
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
	SysGetSysinfo,*/
	SysDebug
};

namespace Scheduler{extern bool schedulerLock;};

void SyscallHandler(regs64_t* regs) {
	//if (regs->rax >= NUM_SYSCALLS) // If syscall is non-existant then return
	//	return;
		
	Scheduler::schedulerLock = true;
	Log::Info("Syscall");
	Log::Info(regs->rax);
	int ret = syscalls[regs->rax](regs); // Call syscall
	regs->rax = ret;
	Scheduler::schedulerLock = false;
}

void InitializeSyscalls() {
	IDT::RegisterInterruptHandler(0x69,SyscallHandler);
}