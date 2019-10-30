#include <syscalls.h>

#include <idt.h>
#include <scheduler.h>
#include <logging.h>
#include <initrd.h>
#include <video.h>
#include <hal.h>
#include <fb.h>

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
	return 0;
}

int SysWrite(regs64_t* r){
	return 0;
}

int SysOpen(regs64_t* r){
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

	uintptr_t fbVirt = (uintptr_t)Memory::Allocate4KPages(vMode.height*vMode.pitch/PAGE_SIZE_4K + 1);
	Memory::MapVirtualMemory4K(HAL::multibootInfo.framebufferAddr,fbVirt,vMode.height*vMode.pitch/PAGE_SIZE_4K + 1);

	fb_info_t fbInfo;
	fbInfo.width = vMode.width;
	fbInfo.height = vMode.height;
	fbInfo.bpp = vMode.bpp;
	fbInfo.pitch = vMode.pitch;

	*((uintptr_t*)r->rbx) = fbVirt;
	*((fb_info_t*)r->rcx) = fbInfo;
}

int SysChmod(regs64_t* r){
	return 0;
}

int SysStat(regs64_t* r){
	return 0;
}

int SysLSeek(regs64_t* r){
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

int SysGetFB(regs64_t* r){

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
	SysChmod,
	SysStat,
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
	if (regs->rax >= NUM_SYSCALLS) // If syscall is non-existant then return
		return;
		
	Scheduler::schedulerLock = true;
	Log::Info("Syscall");
	Log::Info(regs->rax);
	int ret = syscalls[regs->rax](regs); // Call syscall
	if(ret) {
		/*Log::Warning("Syscall was probably not successful!");
		Log::Info(regs->eax, false);
		Log::Info("Exit Code: ");
		Log::Info(ret, false);*/
	}
	regs->rax = ret;
	Scheduler::schedulerLock = false;
}

void InitializeSyscalls() {
	IDT::RegisterInterruptHandler(0x69,SyscallHandler);
}