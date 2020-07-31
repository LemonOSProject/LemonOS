#include <syscalls.h>

#include <errno.h>
#include <idt.h>
#include <scheduler.h>
#include <logging.h>
#include <video.h>
#include <hal.h>
#include <fb.h>
#include <physicalallocator.h>
#include <gui.h>
#include <timer.h>
#include <pty.h>
#include <lock.h>
#include <lemon.h>
#include <sharedmem.h>
#include <cpu.h>
#include <net/socket.h>
#include <timer.h>
#include <lock.h>
#include <smp.h>

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
#define SYS_FSTAT 17
#define SYS_STAT 18
#define SYS_LSEEK 19
#define SYS_GETPID 20
#define SYS_MOUNT 21
#define SYS_MKDIR 22
#define SYS_RMDIR 23
#define SYS_RENAME 24
#define SYS_YIELD 25
#define SYS_READDIR_NEXT 26
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
#define SYS_IOCTL 42
#define SYS_INFO 43
#define SYS_MUNMAP 44
#define SYS_CREATE_SHARED_MEMORY 45
#define SYS_MAP_SHARED_MEMORY 46
#define SYS_UNMAP_SHARED_MEMORY 47
#define SYS_DESTROY_SHARED_MEMORY 48
#define SYS_SOCKET 49
#define SYS_BIND 50
#define SYS_LISTEN 51
#define SYS_ACCEPT 52
#define SYS_CONNECT 53
#define SYS_SEND 54
#define SYS_SENDTO 55
#define SYS_RECEIVE 56
#define SYS_RECEIVEFROM 57
#define SYS_GETUID 58
#define SYS_SETUID 59
#define SYS_POLL 60
#define SYS_SENDMSG 61
#define SYS_RECVMSG 62
#define SYS_GETEUID 63
#define SYS_SETEUID 64
#define SYS_GET_PROCESS_INFO 65
#define SYS_GET_NEXT_PROCESS_INFO 66

#define NUM_SYSCALLS 67

#define EXEC_CHILD 1

typedef long(*syscall_t)(regs64_t*);

long SysExit(regs64_t* r){
	int64_t code = r->rbx;

	Log::Info("Process %d exiting with code %d", Scheduler::GetCurrentProcess()->pid, code);

	Scheduler::EndProcess(Scheduler::GetCurrentProcess());
	return 0;
}

long SysExec(regs64_t* r){
	char* filepath = (char*)kmalloc(strlen((char*)r->rbx) + 1);

	if(!Memory::CheckUsermodePointer(r->rbx, 0, Scheduler::GetCurrentProcess()->addressSpace)) return -1;

	strcpy(filepath, (char*)r->rbx);
	int argc = r->rcx;
	char** argv = (char**)r->rdx;
	uint64_t flags = r->rsi;
	char** envp = (char**)r->rdi;

	FsNode* current_node = fs::ResolvePath(filepath, Scheduler::GetCurrentProcess()->workingDir);

	if(!current_node){
		return 1;
	}

	Log::Info("Loading: %s", (char*)r->rbx);
	timeval_t tv = Timer::GetSystemUptimeStruct();
	uint8_t* buffer = (uint8_t*)kmalloc(current_node->size);
	size_t read = fs::Read(current_node, 0, current_node->size, buffer);
	if(!read){
		Log::Warning("Could not read file: %s", current_node->name);
		return 0;
	}
	timeval_t tvnew = Timer::GetSystemUptimeStruct();
	Log::Info("Done (took %d ms)", Timer::TimeDifference(tvnew, tv));

	char** kernelArgv = (char**)kmalloc(argc * sizeof(char*));
	for(int i = 0; i < argc; i++){
		kernelArgv[i] = (char*)kmalloc(strlen(argv[i] + 1));
		strcpy(kernelArgv[i], argv[i]);
	}
	
	int envCount = 0;
	char** kernelEnvp = nullptr;

	if(envp){
		int i = 0;
        while(envp[i]) i++;
		envCount = i;

		kernelEnvp = (char**)kmalloc(envCount * sizeof(char*));
		for(int i = 0; i < envCount; i++){
			kernelEnvp[i] = (char*)kmalloc(strlen(envp[i] + 1));
			strcpy(kernelEnvp[i], envp[i]);
		}
	}

	process_t* proc = Scheduler::CreateELFProcess((void*)buffer, argc, kernelArgv, envCount, kernelEnvp);
	strncpy(proc->name, fs::BaseName(kernelArgv[0]), NAME_MAX);

	for(int i = 0; i < argc; i++){
		kfree(kernelArgv[i]);
	}
	
	kfree(kernelArgv);
	kfree(buffer);

	if(!proc) return 0;

	if(flags & EXEC_CHILD){
		Scheduler::GetCurrentProcess()->children.add_back(proc);
		proc->parent = Scheduler::GetCurrentProcess();

		proc->fileDescriptors[0] = new fs_fd_t(*Scheduler::GetCurrentProcess()->fileDescriptors.get_at(0));
		proc->fileDescriptors[1] = new fs_fd_t(*Scheduler::GetCurrentProcess()->fileDescriptors.get_at(1));
		proc->fileDescriptors[2] = new fs_fd_t(*Scheduler::GetCurrentProcess()->fileDescriptors.get_at(2));
	}

	strncpy(proc->workingDir, Scheduler::GetCurrentProcess()->workingDir, PATH_MAX);

	return proc->pid;
}

long SysRead(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	if(r->rbx >= proc->fileDescriptors.get_length()){
		Log::Warning("Invalid File Descriptor: %d", r->rbx);
		return -EBADF;
	}
	fs_fd_t* handle = proc->fileDescriptors[r->rbx];
	if(!handle){
		Log::Warning("Invalid File Descriptor: %d", r->rbx);
		return -EBADF;
	}

	uint8_t* buffer = (uint8_t*)r->rcx;
	uint64_t count = r->rdx;

	if(!Memory::CheckUsermodePointer(r->rcx, count, proc->addressSpace)){
		Log::Warning("Invalid Memory Buffer: %x", r->rcx);
		return -EFAULT;
	}

	ssize_t ret = fs::Read(handle, count, buffer);
	return ret;
}

long SysWrite(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();

	if(r->rbx > proc->fileDescriptors.get_length()){
		Log::Warning("Invalid File Descriptor: %d", r->rbx);
		return -EBADF;
	}
	fs_fd_t* handle = proc->fileDescriptors[r->rbx];
	if(!handle){
		Log::Warning("Invalid File Descriptor: %d", r->rbx);
		return -EBADF;
	}

	uint8_t* buffer = (uint8_t*)r->rcx;
	uint64_t count = r->rdx;

	if(!Memory::CheckUsermodePointer(r->rcx, count, proc->addressSpace)){
		Log::Warning("Invalid Memory Buffer: %x", r->rcx);
		return -EFAULT;
	}

	ssize_t ret = fs::Write(handle, r->rdx, buffer);
	return ret;
}

/*
 * SysOpen (path, flags) - Opens file at specified path with flags
 * path - Path of file
 * flags - flags of opened file descriptor
 * 
 * On success: return file descriptor
 * On failure: return -1
 */
long SysOpen(regs64_t* r){
	char* filepath = (char*)kmalloc(strlen((char*)r->rbx) + 1);
	strcpy(filepath, (char*)r->rbx);
	FsNode* root = fs::GetRoot();

	uint64_t flags = r->rcx;

	process_t* proc = Scheduler::GetCurrentProcess();

	//Log::Info("Opening: %s", filepath);
	long fd;
	if(strcmp(filepath,"/") == 0){
		fd = proc->fileDescriptors.get_length();
		proc->fileDescriptors.add_back(fs::Open(root, 0));
		return fd;
	}

open:
	FsNode* node = fs::ResolvePath(filepath, proc->workingDir);

	if(!node){
		if(flags & O_CREAT){
			FsNode* parent = fs::ResolveParent(filepath, proc->workingDir);
			char* basename = fs::BaseName(filepath);

			Log::Info("sys_open: Creating %s", basename);

			if(!parent) {
				Log::Warning("sys_open: Could not resolve parent directory of new file %s", basename);
				return -ENOENT;
			}

			DirectoryEntry ent;
			strcpy(ent.name, basename);
			parent->Create(&ent, flags);

			kfree(basename);

			flags &= ~O_CREAT;
			goto open;
		} else {
			Log::Warning("sys_open: Failed to open file %s", filepath);
			return -ENOENT;
		}
	}

	if(flags & O_DIRECTORY && ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY)){
		return -ENOTDIR;
	}

	if(flags & O_TRUNC && ((flags & O_ACCESS) == O_RDWR || (flags & O_ACCESS) == O_WRONLY)){
		node->Truncate(0);
	}

	fs_fd_t* handle = fs::Open(node, r->rcx);

	fd = Scheduler::GetCurrentProcess()->fileDescriptors.get_length();
	Scheduler::GetCurrentProcess()->fileDescriptors.add_back(handle);
	fs::Open(node, flags);

	return fd;
}

long SysClose(regs64_t* r){
	int fd = r->rbx;
	
	if(fd >= static_cast<int>(Scheduler::GetCurrentProcess()->fileDescriptors.get_length())){
		Log::Warning("sys_close: Invalid File Descriptor, %d", fd);
		return -EINVAL;
	}

	fs_fd_t* handle;

	if((handle = Scheduler::GetCurrentProcess()->fileDescriptors[fd])){
		fs::Close(handle);
	}

	handle->node = nullptr;

	Scheduler::GetCurrentProcess()->fileDescriptors[fd] = nullptr;
	return 0;
}

long SysSleep(regs64_t* r){
	return 0;
}

long SysCreate(regs64_t* r){
	return 0;
}

long SysLink(regs64_t* r){
	const char* oldpath = (const char*)r->rbx;
	const char* newpath = (const char*)r->rcx;

	process_t* proc = Scheduler::GetCurrentProcess();

	if(!(Memory::CheckUsermodePointer(r->rbx, 1, proc->addressSpace) && Memory::CheckUsermodePointer(r->rcx, 1, proc->addressSpace))){
		Log::Warning("sys_link: Invalid path pointer");
		return -EFAULT;
	}

	FsNode* file = fs::ResolvePath(oldpath);
	if(!file){
		Log::Warning("sys_link: Could not resolve path: %s", oldpath);
		return -ENOENT;
	}
	
	FsNode* parentDirectory = fs::ResolveParent(newpath, proc->workingDir);
	char* linkName = fs::BaseName(newpath);
	
	Log::Info("sys_link: Attempting to create link %s at path %s", linkName, newpath);

	if(!parentDirectory){
		Log::Warning("sys_link: Could not resolve path: %s", newpath);
		return -ENOENT;
	}

	if((parentDirectory->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
		Log::Warning("sys_link: Could not resolve path: Parent not a directory: %s", newpath);
		return -ENOTDIR;
	}

	DirectoryEntry entry;
	strcpy(entry.name, linkName);
	return parentDirectory->Link(file, &entry);
}

long SysUnlink(regs64_t* r){
	const char* path = (const char*)r->rbx;
	
	process_t* proc = Scheduler::GetCurrentProcess();

	if(!Memory::CheckUsermodePointer(r->rbx, 1, proc->addressSpace)){
		Log::Warning("sys_unlink: Invalid path pointer");
		return -EFAULT;
	}

	FsNode* parentDirectory = fs::ResolveParent(path, proc->workingDir);
	char* linkName = fs::BaseName(path);

	if(!parentDirectory){
		Log::Warning("sys_unlink: Could not resolve path: %s", path);
		return -EINVAL;
	}

	if((parentDirectory->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
		Log::Warning("sys_unlink: Could not resolve path: Not a directory: %s", path);
		return -ENOTDIR;
	}

	DirectoryEntry entry;
	strcpy(entry.name, linkName);
	return parentDirectory->Unlink(&entry);
}

long SysChdir(regs64_t* r){
	if(r->rbx){
		char* path =  fs::CanonicalizePath((char*)r->rbx, Scheduler::GetCurrentProcess()->workingDir);
		FsNode* n = fs::ResolvePath(path);
		if(!n) {
			Log::Warning("chdir: Could not find %s", path);
			return -1;
		} else if (n->flags != FS_NODE_DIRECTORY){
			return -ENOTDIR;
		}
		strcpy(Scheduler::GetCurrentProcess()->workingDir, path);
	} else Log::Warning("chdir: Invalid path string");
	return 0;
}

long SysTime(regs64_t* r){
	return 0;
}

long SysMapFB(regs64_t *r){
	video_mode_t vMode = Video::GetVideoMode();

	uint64_t pageCount = (vMode.height * vMode.pitch + 0xFFF) >> 12;
	uintptr_t fbVirt = (uintptr_t)Memory::Allocate4KPages(pageCount, Scheduler::GetCurrentProcess()->addressSpace);
	Memory::MapVirtualMemory4K(HAL::multibootInfo.framebufferAddr,fbVirt,pageCount,Scheduler::GetCurrentProcess()->addressSpace);

	mem_region_t memR;
	memR.base = fbVirt;
	memR.pageCount = pageCount;
	Scheduler::GetCurrentProcess()->sharedMemory.add_back(memR);

	fb_info_t fbInfo;
	fbInfo.width = vMode.width;
	fbInfo.height = vMode.height;
	fbInfo.bpp = vMode.bpp;
	fbInfo.pitch = vMode.pitch;

	if(HAL::debugMode) fbInfo.height = vMode.height / 3 * 2;

	Log::Info("Mapping Framebuffer to:");
	Log::Info(fbVirt);

	*((uintptr_t*)r->rbx) = fbVirt;
	*((fb_info_t*)r->rcx) = fbInfo;

	Log::Info(r->rbx);
	Log::Info(r->rcx);
	Log::Info(*((uint64_t*)r->rbx));

	return 0;
}

long SysAlloc(regs64_t* r){
	uint64_t pageCount = r->rbx;
	uintptr_t* addressPointer = (uintptr_t*)r->rcx;

	uintptr_t address = (uintptr_t)Memory::Allocate4KPages(pageCount, Scheduler::GetCurrentProcess()->addressSpace);

	assert(address);

	for(unsigned i = 0; i < pageCount; i++){
		Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(),address + i * PAGE_SIZE_4K,1,Scheduler::GetCurrentProcess()->addressSpace);
		memset((void*)(address + i * PAGE_SIZE_4K), 0, PAGE_SIZE_4K);
	}

	*addressPointer = address;

	return 0;
}

long SysChmod(regs64_t* r){
	return 0;
}

long SysFStat(regs64_t* r){
	stat_t* stat = (stat_t*)r->rbx;
	int fd = r->rcx;

	if(fd >= static_cast<int>(Scheduler::GetCurrentProcess()->fileDescriptors.get_length())){
		Log::Warning("sys_fstat: Invalid File Descriptor, %d", fd);
		return -EBADF;
	}
	FsNode* node = Scheduler::GetCurrentProcess()->fileDescriptors.get_at(fd)->node;
	if(!node){
		Log::Warning("sys_fstat: Invalid File Descriptor, %d", fd);
		return -EBADF;
	}

	stat->st_dev = 0;
	stat->st_ino = node->inode;
	stat->st_mode = 0;
	
	if(node->flags == FS_NODE_DIRECTORY) stat->st_mode |= S_IFDIR;
	if(node->flags == FS_NODE_FILE) stat->st_mode |= S_IFREG;
	if(node->flags == FS_NODE_BLKDEVICE) stat->st_mode |= S_IFBLK;
	if(node->flags == FS_NODE_CHARDEVICE) stat->st_mode |= S_IFCHR;
	if(node->flags == FS_NODE_SYMLINK) stat->st_mode |= S_IFLNK;
	if(node->flags == FS_NODE_SOCKET) stat->st_mode |= S_IFSOCK;

	stat->st_nlink = 0;
	stat->st_uid = node->uid;
	stat->st_gid = 0;
	stat->st_rdev = 0;
	stat->st_size = node->size;
	stat->st_blksize = 0;
	stat->st_blocks = 0;

	return 0;
}

long SysStat(regs64_t* r){
	stat_t* stat = (stat_t*)r->rbx;
	char* filepath = (char*)r->rcx;
	process_t* proc = Scheduler::GetCurrentProcess();

	if(!Memory::CheckUsermodePointer(r->rbx, sizeof(stat_t), proc->addressSpace)){
		Log::Warning("sys_stat: stat structure points to invalid address %x", r->rbx);
	}
	
	if(!Memory::CheckUsermodePointer(r->rcx, 1, proc->addressSpace)){
		Log::Warning("sys_stat: filepath points to invalid address %x", r->rbx);
	}

	FsNode* node = fs::ResolvePath(filepath, proc->workingDir);

	if(!node){
		Log::Warning("sys_stat: Invalid filepath %s", filepath);
		return -ENOENT;
	}

	stat->st_dev = 0;
	stat->st_ino = node->inode;
	stat->st_mode = 0;
	
	if(node->flags == FS_NODE_DIRECTORY) stat->st_mode |= S_IFDIR;
	if(node->flags == FS_NODE_FILE) stat->st_mode |= S_IFREG;
	if(node->flags == FS_NODE_BLKDEVICE) stat->st_mode |= S_IFBLK;
	if(node->flags == FS_NODE_CHARDEVICE) stat->st_mode |= S_IFCHR;
	if(node->flags == FS_NODE_SYMLINK) stat->st_mode |= S_IFLNK;
	if(node->flags == FS_NODE_SOCKET) stat->st_mode |= S_IFSOCK;

	stat->st_nlink = 0;
	stat->st_uid = node->uid;
	stat->st_gid = 0;
	stat->st_rdev = 0;
	stat->st_size = node->size;
	stat->st_blksize = 0;
	stat->st_blocks = 0;

	return 0;
}

long SysLSeek(regs64_t* r){
	long ret = 0;
	int fd = r->rbx;

	if(fd >= static_cast<int>(Scheduler::GetCurrentProcess()->fileDescriptors.get_length()) || !Scheduler::GetCurrentProcess()->fileDescriptors[fd]){
		Log::Warning("sys_lseek: Invalid File Descriptor, %d", fd);
		return -1;
	}

	switch(r->rdx){
	case 0: // SEEK_SET
		ret = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->pos = r->rcx;
		return ret;
		break;
	case 1: // SEEK_CUR
		ret = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->pos;
		return ret;
		break;
	case 2: // SEEK_END
		ret = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->pos = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->node->size;
		return ret;
		break;
	default:
		Log::Info("Invalid seek: %s, mode: %d", Scheduler::GetCurrentProcess()->fileDescriptors[fd]->node->name, r->rdx);
		return -1; // Invalid seek mode
		break;
	}

	return ret;
}

long SysGetPID(regs64_t* r){
	uint64_t* pid = (uint64_t*)r->rbx;

	*pid = Scheduler::GetCurrentProcess()->pid;
	
	return 0;
}

long SysMount(regs64_t* r){
	return 0;
}

long SysMkdir(regs64_t* r){
	char* path = (char*)r->rbx;
	mode_t mode = r->rcx;

	process_t* proc = Scheduler::GetCurrentProcess();

	if(!Memory::CheckUsermodePointer(r->rbx, 0, proc->addressSpace)){
		Log::Warning("sys_mkdir: Invalid path pointer %x", r->rbx);
		return -EFAULT;
	}

	FsNode* parentDirectory = fs::ResolveParent(path, proc->workingDir);
	char* dirPath = fs::BaseName(path);
	
	Log::Info("sys_mkdir: Attempting to create %s at path %s", dirPath, path);

	if(!parentDirectory){
		Log::Warning("sys_mkdir: Could not resolve path: %s", path);
		return -EINVAL;
	}

	if((parentDirectory->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
		Log::Warning("sys_mkdir: Could not resolve path: Not a directory: %s", path);
		return -ENOTDIR;
	}

	DirectoryEntry dir;
	strcpy(dir.name, dirPath);
	int ret = parentDirectory->CreateDirectory(&dir, mode);

	return ret;
}

long SysRmdir(regs64_t* r){
	Log::Warning("SysRmdir is a stub!");
	return -ENOSYS;
}

long SysRename(regs64_t* r){
	char* oldpath = (char*)r->rbx;
	char* newpath = (char*)r->rcx;

	process_t* proc = Scheduler::GetCurrentProcess();

	if(!Memory::CheckUsermodePointer(r->rbx, 0, proc->addressSpace)){
		Log::Warning("sys_rename: Invalid oldpath pointer %x", r->rbx);
		return -EFAULT;
	}
	
	if(!Memory::CheckUsermodePointer(r->rcx, 0, proc->addressSpace)){
		Log::Warning("sys_rename: Invalid newpath pointer %x", r->rbx);
		return -EFAULT;
	}

	FsNode* olddir = fs::ResolveParent(oldpath, proc->workingDir);
	FsNode* newdir = fs::ResolveParent(newpath, proc->workingDir);

	if(!(olddir && newdir)){
		return -ENOENT;
	}

	return fs::Rename(olddir, fs::BaseName(oldpath), newdir, fs::BaseName(newpath));
}

long SysYield(regs64_t* r){
	Scheduler::Yield();
	return 0;
}

/*
 * SysReadDirNext(fd, direntPointer) - Read directory using the file descriptor offset as an index
 * 
 * fd - File descriptor of directory
 * direntPointer - Pointer to fs_dirent_t
 * 
 * Return Value:
 * 1 on Success
 * 0 on End of directory
 * Negative value on failure
 * 
 */
long SysReadDirNext(regs64_t* r){
	unsigned int fd = r->rbx;
	if(fd > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
		return -EBADF;
	} 
	
	fs_dirent_t* direntPointer = (fs_dirent_t*)r->rcx;
	fs_fd_t* handle = Scheduler::GetCurrentProcess()->fileDescriptors[fd];

	if(!handle){
		return -EBADF;
	}

	if(!Memory::CheckUsermodePointer((uintptr_t)direntPointer, sizeof(fs_dirent_t), Scheduler::GetCurrentProcess()->addressSpace)){
		return -EFAULT;
	}

	if((Scheduler::GetCurrentProcess()->fileDescriptors[fd]->node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
		return -ENOTDIR;
	}

	DirectoryEntry tempent;
	int ret = fs::ReadDir(handle, &tempent, handle->pos++);

	strcpy(direntPointer->name, tempent.name);
	direntPointer->type = tempent.flags;

	return ret;
}

long SysRenameAt(regs64_t* r){
	Log::Warning("SysRenameAt is a stub!");
	return -ENOSYS;
}

// SendMessage(message_t* msg) - Sends an IPC message to a process
long SysSendMessage(regs64_t* r){
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
long SysReceiveMessage(regs64_t* r){
	if(!(r->rbx && r->rcx)) return 1; // Was given null pointers

	message_t* msg = (message_t*)r->rbx;
	uint64_t* queueSize = (uint64_t*)r->rcx;

	*queueSize = Scheduler::GetCurrentProcess()->messageQueue.get_length();
	*msg = Scheduler::RecieveMessage(Scheduler::GetCurrentProcess());

	return 0;
}

long SysUptime(regs64_t* r){
	uint64_t* seconds = (uint64_t*)r->rbx;
	uint64_t* milliseconds = (uint64_t*)r->rcx;
	if(seconds){
		*seconds = Timer::GetSystemUptime();
	}
	if(milliseconds){
		*milliseconds = ((double)Timer::GetTicks())/(Timer::GetFrequency()/1000.0);
	}
	return 0;
}

long SysDebug(regs64_t* r){
	Log::Info("%s, %d", (char*)r->rbx, r->rcx);
	return 0;
}

long SysGetVideoMode(regs64_t* r){
	video_mode_t vMode = Video::GetVideoMode();
	fb_info_t fbInfo;
	fbInfo.width = vMode.width;
	fbInfo.height = vMode.height;
	if(HAL::debugMode) fbInfo.height = vMode.height / 3 * 2;
	fbInfo.bpp = vMode.bpp;
	fbInfo.pitch = vMode.pitch;

	*((fb_info_t*)r->rbx) = fbInfo;

	return 0;
}

long SysUName(regs64_t* r){
	char* str = (char*)r->rbx;
	strcpy(str, Lemon::versionString);

	return 0;
}

long SysReadDir(regs64_t* r){
	int fd = r->rbx;

	if(fd >= static_cast<int>(Scheduler::GetCurrentProcess()->fileDescriptors.get_length())){
		return -EBADF;
	} 
	
	fs_dirent_t* direntPointer = (fs_dirent_t*)r->rcx;

	if(!Memory::CheckUsermodePointer(r->rcx, sizeof(fs_dirent_t), Scheduler::GetCurrentProcess()->addressSpace)){
		return -EFAULT;
	}

	unsigned int count = r->rdx;

	if((Scheduler::GetCurrentProcess()->fileDescriptors[fd]->node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
		return -ENOTDIR;
	}

	DirectoryEntry tempent;
	int ret = fs::ReadDir(Scheduler::GetCurrentProcess()->fileDescriptors[fd], &tempent, count);

	strcpy(direntPointer->name, tempent.name);
	direntPointer->type = tempent.flags;

	return ret;
}

long SysSetFsBase(regs64_t* r){
	asm volatile ("wrmsr" :: "a"(r->rbx & 0xFFFFFFFF) /*Value low*/, "d"((r->rbx >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000100) /*Set FS Base*/);
	GetCPULocal()->currentThread->fsBase = r->rbx;
	return 0;
}

long SysMmap(regs64_t* r){
	uint64_t* address = (uint64_t*)r->rbx;
	size_t count = r->rcx;
	uintptr_t hint = r->rdx;

	uintptr_t _address;
	if(hint){
		if(Memory::CheckRegion(hint, count * PAGE_SIZE_4K, Scheduler::GetCurrentProcess()->addressSpace) /*Check availibilty of the requested map*/){
			_address = hint;
		} else {
			Log::Warning("sys_mmap: Could not map to address %x", hint);
			*address = 0;
			return 1;
		}
	} else _address = (uintptr_t)Memory::Allocate4KPages(count, Scheduler::GetCurrentProcess()->addressSpace);

	for(size_t i = 0; i < count; i++){
		Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), _address + i * PAGE_SIZE_4K, 1, Scheduler::GetCurrentProcess()->addressSpace);
		memset((void*)(_address + i * PAGE_SIZE_4K), 0, PAGE_SIZE_4K);
	}

	*address = _address;

	return 0;
}

long SysGrantPTY(regs64_t* r){
	if(!r->rbx) return 1;

	PTY* pty = GrantPTY(Scheduler::GetCurrentProcess()->pid);

	process_t* currentProcess = Scheduler::GetCurrentProcess();

	*((int*)r->rbx) = currentProcess->fileDescriptors.get_length();
	
	currentProcess->fileDescriptors[0] = fs::Open(&pty->slaveFile); // Stdin
	currentProcess->fileDescriptors[1] = fs::Open(&pty->slaveFile); // Stdout
	currentProcess->fileDescriptors[2] = fs::Open(&pty->slaveFile); // Stderr

	currentProcess->fileDescriptors.add_back(fs::Open(&pty->masterFile));

	return 0;
}

long SysGetCWD(regs64_t* r){
	char* buf = (char*)r->rbx;
	size_t sz = r->rcx;

	char* workingDir = Scheduler::GetCurrentProcess()->workingDir;
	if(strlen(workingDir) > sz) {
		return 1;
	} else {
		strcpy(buf, workingDir);
		return 0;
	}

	return 0;
}

long SysWaitPID(regs64_t* r){
	uint64_t pid = r->rbx;

	lock_t unused = 0;
	process_t* proc = nullptr;

	if((proc = Scheduler::FindProcessByPID(pid))){
		Scheduler::BlockCurrentThread(proc->blocking, unused);
	}

	while((proc = Scheduler::FindProcessByPID(pid))) { Scheduler::Yield(); }

	return 0;
}

long SysNanoSleep(regs64_t* r){
	uint64_t nanoseconds = r->rbx;

	uint64_t ticks = nanoseconds * Timer::GetFrequency() / 1000000000;
	Timer::SleepCurrentThread(ticks);

	return 0;
}

long SysPRead(regs64_t* r){
	if(r->rbx > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
		return -EBADF;
	}
	FsNode* node;
	if(Scheduler::GetCurrentProcess()->fileDescriptors.get_at(r->rbx) || !(node = Scheduler::GetCurrentProcess()->fileDescriptors.get_at(r->rbx)->node)){ 
		Log::Warning("sys_pread: Invalid file descriptor: %d", r->rbx);
		return -EBADF; 
	}
	
	if(!Memory::CheckUsermodePointer(r->rcx, r->rdx, Scheduler::GetCurrentProcess()->addressSpace)) {
		return -EFAULT;
	}

	uint8_t* buffer = (uint8_t*)r->rcx;
	uint64_t count = r->rdx;
	uint64_t off = r->rdi;
	return fs::Read(node, off, count, buffer);
}

long SysPWrite(regs64_t* r){
	if(r->rbx > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
		return -EBADF;
	}
	FsNode* node;
	if(Scheduler::GetCurrentProcess()->fileDescriptors.get_at(r->rbx) || !(node = Scheduler::GetCurrentProcess()->fileDescriptors.get_at(r->rbx)->node)){ 
		Log::Warning("sys_pwrite: Invalid file descriptor: %d", r->rbx);
		return -EBADF;
	}

	if(!Memory::CheckUsermodePointer(r->rcx, r->rdx, Scheduler::GetCurrentProcess()->addressSpace)) {
		return -EFAULT;
	}

	uint64_t off = r->rdi;

	return fs::Write(node, off, r->rdx, (uint8_t*)r->rcx);
}

long SysIoctl(regs64_t* r){
	int fd = r->rbx;
	uint64_t request = r->rcx;
	uint64_t arg = r->rdx;
	int* result = (int*)r->rsi;

	if(fd >= static_cast<int>(Scheduler::GetCurrentProcess()->fileDescriptors.get_length())){
		return -1;
	}
	fs_fd_t* handle = Scheduler::GetCurrentProcess()->fileDescriptors[r->rbx];
	if(!handle){
		Log::Warning("sys_ioctl: Invalid File Descriptor: %d", r->rbx);
		return -2;
	}

	int ret = fs::Ioctl(handle, request, arg);

	if(result) *result = ret;

	return ret;
}

long SysInfo(regs64_t* r){
	lemon_sysinfo_t* s = (lemon_sysinfo_t*)r->rbx;

	if(!s){
		return -1;
	}

	s->usedMem = Memory::usedPhysicalBlocks * 4;
	s->totalMem = Memory::maxPhysicalBlocks * 4;
	s->cpuCount = static_cast<uint16_t>(SMP::processorCount);

	return 0;
}

/*
 * SysMunmap - Unmap memory (addr, count)
 * 
 * On success - return 0
 * On failure - return -1
 */
long SysMunmap(regs64_t* r){
	uint64_t address = r->rbx;
	size_t count = r->rcx;
	
	if(Memory::CheckRegion(address, count * PAGE_SIZE_4K, Scheduler::GetCurrentProcess()->addressSpace) /*Check availibilty of the requested map*/){
		//Memory::Free4KPages((void*)address, count, Scheduler::GetCurrentProcess()->addressSpace);
	} else {
		return -1;
	}

	return 0;
}

/* 
 * SysCreateSharedMemory (key, size, flags, recipient) - Create Shared Memory
 * key - Pointer to memory key
 * size - memory size
 * flags - flags
 * recipient - (if private flag) PID of the process that can access memory
 *
 * On Success - Return 0, key greater than 1
 * On Failure - Return -1, key null
 */
long SysCreateSharedMemory(regs64_t* r){
	uint64_t* key = (uint64_t*)r->rbx;
	uint64_t size = r->rcx;
	uint64_t flags = r->rdx;
	uint64_t recipient = r->rsi;

	*key = Memory::CreateSharedMemory(size, flags, Scheduler::GetCurrentProcess()->pid, recipient);

	if(!*key) return -1; // Failed

	return 0;
}

/* 
 * SysMapSharedMemory (ptr, key, hint) - Map Shared Memory
 * ptr - Pointer to pointer of mapped memory
 * key - Memory key
 * key - Address hint
 *
 * On Success - ptr > 0
 * On Failure - ptr = 0
 */
long SysMapSharedMemory(regs64_t* r){
	void** ptr = (void**)r->rbx;
	uint64_t key = r->rcx;
	uint64_t hint = r->rdx;

	*ptr = Memory::MapSharedMemory(key,Scheduler::GetCurrentProcess(), hint);

	return 0;
}

/* 
 * SysUnmapSharedMemory (address, key) - Map Shared Memory
 * address - address of mapped memory
 * key - Memory key
 *
 * On Success - return 0
 * On Failure - return -1
 */
long SysUnmapSharedMemory(regs64_t* r){
	uint64_t address = r->rbx;
	uint64_t key = r->rcx;

	shared_mem_t* sMem = Memory::GetSharedMemory(key);
	if(!sMem) return -1;

	if(!Memory::CheckRegion(address, sMem->pgCount * PAGE_SIZE_4K, Scheduler::GetCurrentProcess()->addressSpace)) // Make sure the process is not screwing with kernel memory
		return -1;

	Memory::Free4KPages((void*)address, sMem->pgCount, Scheduler::GetCurrentProcess()->addressSpace);

	sMem->mapCount--;

	Memory::DestroySharedMemory(key); // Active shared memory will not be destroyed and this will return

	return 0;
}

/* 
 * SysDestroySharedMemory (key) - Destroy Shared Memory
 * key - Memory key
 *
 * On Success - return 0
 * On Failure - return -1
 */
long SysDestroySharedMemory(regs64_t* r){
	uint64_t key = r->rbx;

	if(Memory::CanModifySharedMemory(Scheduler::GetCurrentProcess()->pid, key)){
		Memory::DestroySharedMemory(key);
	} else return -1;

	return 0;
}

/* 
 * SysSocket (domain, type, protocol) - Create socket
 * domain - socket domain
 * type - socket type
 * protcol - socket protocol
 *
 * On Success - return file descriptor
 * On Failure - return -1
 */
long SysSocket(regs64_t* r){
	int domain = r->rbx;
	int type = r->rcx;
	int protocol = r->rdx;

	Socket* sock = Socket::CreateSocket(domain, type, protocol);

	if(!sock) return -1;
	
	fs_fd_t* fDesc = fs::Open(sock, 0);

	if(type & SOCK_NONBLOCK) fDesc->mode |= O_NONBLOCK;

	int fd = Scheduler::GetCurrentProcess()->fileDescriptors.get_length();

	Scheduler::GetCurrentProcess()->fileDescriptors.add_back(fDesc);

	return fd;
}

/* 
 * SysBind (sockfd, addr, addrlen) - Bind address to socket
 * sockfd - Socket file descriptor
 * addr - sockaddr structure
 * addrlen - size of addr
 *
 * On Success - return 0
 * On Failure - return -1
 */
long SysBind(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);
	if(!handle){ 
		Log::Warning("sys_bind: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_bind: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}

	socklen_t len = r->rdx;
	
	sockaddr_t* addr = (sockaddr_t*)r->rcx;
	if(!Memory::CheckUsermodePointer(r->rcx, len, proc->addressSpace)){
		Log::Warning("sys_bind: Invalid sockaddr ptr");
		return -3;
	}

	Socket* sock = (Socket*)handle->node;
	return sock->Bind(addr, len);
}

/* 
 * SysListen (sockfd, backlog) - Marks socket as passive
 * sockfd - socket file descriptor
 * backlog - maximum amount of pending connections
 *
 * On Success - return 0
 * On Failure - return -1
 */
long SysListen(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);
	if(!handle){ 
		Log::Warning("sys_listen: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_listen: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}

	Socket* sock = (Socket*)handle->node;
	return sock->Listen(r->rcx);
}

/* 
 * SysAccept (sockfd, addr, addrlen) - Accept socket connection
 * sockfd - Socket file descriptor
 * addr - sockaddr structure
 * addrlen - size of addr
 *
 * On Success - return file descriptor of accepted socket
 * On Failure - return -1
 */
long SysAccept(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);
	if(!handle){ 
		Log::Warning("sys_accept: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_accept: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}

	socklen_t* len = (socklen_t*)r->rdx;
	if(!Memory::CheckUsermodePointer(r->rdx, sizeof(socklen_t), proc->addressSpace)){
		Log::Warning("sys_accept: Invalid socklen ptr");
		return -3;
	}
	
	sockaddr_t* addr = (sockaddr_t*)r->rcx;
	if(!Memory::CheckUsermodePointer(r->rcx, *len, proc->addressSpace)){
		Log::Warning("sys_accept: Invalid sockaddr ptr");
		return -3;
	}

	Socket* sock = (Socket*)handle->node;
	
	Socket* newSock = sock->Accept(addr, len, handle->mode);
	if(!newSock){
		return -1;
	}

	int fd = proc->fileDescriptors.get_length();
	fs_fd_t* newHandle = fs::Open(newSock);
	proc->fileDescriptors.add_back(newHandle);

	if(newSock)
		return fd;
	else
		return -1;
}

/* 
 * SysConnect (sockfd, addr, addrlen) - Initiate socket connection
 * sockfd - Socket file descriptor
 * addr - sockaddr structure
 * addrlen - size of addr
 *
 * On Success - return 0
 * On Failure - return -1
 */
long SysConnect(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);
	if(!handle){ 
		Log::Warning("sys_connect: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_connect: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}

	socklen_t len = r->rdx;
	
	sockaddr_t* addr = (sockaddr_t*)r->rcx;
	if(!Memory::CheckUsermodePointer(r->rcx, len, proc->addressSpace)){
		Log::Warning("sys_connect: Invalid sockaddr ptr");
		return -3;
	}

	Socket* sock = (Socket*)handle->node;
	return sock->Connect(addr, len);
}

/* 
 * SysSend (sockfd, buf, len, flags) - Send data through a socket
 * sockfd - Socket file descriptor
 * buf - data
 * len - data length
 * flags - flags
 *
 * On Success - return amount of data sent
 * On Failure - return -1
 */
long SysSend(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);

	uint8_t* buffer = (uint8_t*)(r->rcx);
	size_t len = r->rdx;
	uint64_t flags = r->rsi;

	if(!handle){ 
		Log::Warning("sys_send: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_send: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}
	
	if(!Memory::CheckUsermodePointer(r->rcx, len, proc->addressSpace)){
		Log::Warning("sys_send: Invalid buffer ptr");
		return -3;
	}

	Socket* sock = (Socket*)handle->node;
	return sock->Send(buffer, len, flags);
}

/* 
 * SysSendTo (sockfd, buf, len, flags, destaddr, addrlen) - Send data through a socket
 * sockfd - Socket file descriptor
 * buf - data
 * len - data length
 * destaddr - Destination address
 * addrlen - Address length
 *
 * On Success - return amount of data sent
 * On Failure - return -1
 */
long SysSendTo(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);

	uint8_t* buffer = (uint8_t*)(r->rcx);
	size_t len = r->rdx;
	uint64_t flags = r->rsi;

	if(!handle){ 
		Log::Warning("sys_send: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_send: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}
	
	if(!Memory::CheckUsermodePointer(r->rcx, len, proc->addressSpace)){
		Log::Warning("sys_send: Invalid buffer ptr");
		return -3;
	}
	
	socklen_t slen = r->rdx;
	
	sockaddr_t* addr = (sockaddr_t*)r->rcx;

	Socket* sock = (Socket*)handle->node;
	return sock->SendTo(buffer, len, flags, addr, slen);
}

/* 
 * SysReceive (sockfd, buf, len, flags) - Receive data through a socket
 * sockfd - Socket file descriptor
 * buf - data
 * len - data length
 * flags - flags
 *
 * On Success - return amount of data read
 * On Failure - return -1
 */
long SysReceive(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);

	uint8_t* buffer = (uint8_t*)(r->rcx);
	size_t len = r->rdx;
	uint64_t flags = r->rsi;

	if(!handle){ 
		Log::Warning("sys_send: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_send: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}
	
	if(!Memory::CheckUsermodePointer(r->rcx, len, proc->addressSpace)){
		Log::Warning("sys_send: Invalid buffer ptr");
		return -3;
	}

	Socket* sock = (Socket*)handle->node;
	return sock->Receive(buffer, len, flags);
}

/* 
 * SysReceiveFrom (sockfd, buf, len, flags, srcaddr, addrlen) - Receive data through a socket
 * sockfd - Socket file descriptor
 * buf - data
 * len - data length
 * srcaddr - Source address
 * addrlen - Address length
 *
 * On Success - return amount of data read
 * On Failure - return -1
 */
long SysReceiveFrom(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);

	uint8_t* buffer = (uint8_t*)(r->rcx);
	size_t len = r->rdx;
	uint64_t flags = r->rsi;

	if(!handle){ 
		Log::Warning("sys_send: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_send: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}
	
	if(!Memory::CheckUsermodePointer(r->rcx, len, proc->addressSpace)){
		Log::Warning("sys_send: Invalid buffer ptr");
		return -3;
	}
	
	socklen_t* slen = (socklen_t*)r->rdx;
	
	sockaddr_t* addr = (sockaddr_t*)r->rcx;

	Socket* sock = (Socket*)handle->node;
	return sock->ReceiveFrom(buffer, len, flags, addr, slen);
}

/* 
 * SetGetUID () - Get Process UID
 * 
 * On Success - Return process UID
 * On Failure - Does not fail
 */
long SysGetUID(regs64_t* r){
	return Scheduler::GetCurrentProcess()->uid;
}

/* 
 * SetSetUID () - Set Process UID
 * 
 * On Success - Return process UID
 * On Failure - Return negative value
 */
long SysSetUID(regs64_t* r){
	return -ENOSYS;
}

/* 
 * SysPoll (fds, nfds, timeout) - Wait for file descriptors
 * fds - Array of pollfd structs
 * nfds - Number of pollfd structs
 * timeout - timeout period
 *
 * On Success - return number of file descriptors
 * On Failure - return -1
 */
long SysPoll(regs64_t* r){
	pollfd* fds = (pollfd*)r->rbx;
	unsigned nfds = r->rcx;
	int timeout = r->rdx;

	process_t* proc = Scheduler::GetCurrentProcess();
	if(!Memory::CheckUsermodePointer(r->rbx, nfds * sizeof(pollfd), proc->addressSpace)){
		Log::Warning("sys_poll: Invalid pointer to file descriptor array");
	}

	fs_fd_t** files = (fs_fd_t**)kmalloc(sizeof(fs_fd_t*) * nfds);

	unsigned eventCount = 0; // Amount of fds with evetns
	for(unsigned i = 0; i < nfds; i++){
		fds[i].revents = 0;
		if(fds[i].fd < 0) continue;

		if((uint64_t)fds[i].fd > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
			Log::Warning("sys_poll: Invalid File Descriptor: %d", fds[i].fd);
			files[i] = 0;
			fds[i].revents |= POLLNVAL;
			eventCount++;
			continue;
		}

		fs_fd_t* handle = Scheduler::GetCurrentProcess()->fileDescriptors[fds[i].fd];

		if(!handle || !handle->node){
			Log::Warning("sys_poll: Invalid File Descriptor: %d", fds[i].fd);
			files[i] = 0;
			fds[i].revents |= POLLNVAL;
			eventCount++;
			continue;
		}

		files[i] = handle;

		bool hasEvent = 0;

		if((files[i]->node->flags & FS_NODE_TYPE) == FS_NODE_SOCKET){
			if(!((Socket*)files[i]->node)->IsConnected()){
				fds[i].revents |= POLLHUP;
				hasEvent = true;
			}
		}

		if(files[i]->node->CanRead() && (fds[i].events & POLLIN)) { // If readable and the caller requested POLLIN
			fds[i].revents |= POLLIN;
			hasEvent = true;
		}
		
		if(files[i]->node->CanWrite() && (fds[i].events & POLLOUT)) { // If writable and the caller requested POLLOUT
			fds[i].revents |= POLLOUT;
			hasEvent = true;
		}

		if(hasEvent) eventCount++;
	}

	if(timeout){
		int ticksPerMs = (1000 / Timer::GetFrequency());
		unsigned endMs = Timer::GetSystemUptime() * 1000 + ticksPerMs * (Timer::GetTicks() + timeout);
		while(((Timer::GetSystemUptime() * 1000 + ticksPerMs * Timer::GetTicks()) < endMs) || (timeout < 0)){ // Wait until timeout, unless timeout is negative in which wait infinitely
			if(eventCount > 0){
				break;
			}

			for(unsigned i = 0; i < nfds; i++){
				if(!files[i] || !files[i]->node) continue;

				bool hasEvent = 0;

				if(files[i]->node->flags & FS_NODE_SOCKET){
					if(!((Socket*)files[i]->node)->IsConnected()){
						fds[i].revents |= POLLHUP;
						hasEvent = 1;
					}
				}

				if(files[i]->node->CanRead() && (fds[i].events & POLLIN)) { // If readable and the caller requested POLLIN
					fds[i].revents |= POLLIN;
					hasEvent = 1;
				}
				
				if(files[i]->node->CanWrite() && (fds[i].events & POLLOUT)) { // If writable and the caller requested POLLOUT
					fds[i].revents |= POLLOUT;
					hasEvent = 1;
				}

				if(hasEvent) eventCount++;
			}
			Scheduler::Yield();
		}
	}

	kfree(files);

	return eventCount;
}

/* 
 * SysSend (sockfd, msg, flags) - Send data through a socket
 * sockfd - Socket file descriptor
 * msg - Message Header
 * flags - flags
 *
 * On Success - return amount of data sent
 * On Failure - return -1
 */
long SysSendMsg(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();

	if(r->rbx >= proc->fileDescriptors.get_length()){
		Log::Warning("sys_sendmsg: Invalid File Descriptor: %d", r->rbx);
		return -1;
	}

	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);

	msghdr* msg = (msghdr*)r->rcx;
	uint64_t flags = r->rsi;

	if(!handle){ 
		Log::Warning("sys_sendmsg: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_sendmsg: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}
	
	if(!Memory::CheckUsermodePointer(r->rcx, sizeof(msghdr), proc->addressSpace)){
		Log::Warning("sys_sendmsg: Invalid msg ptr");
		return -EFAULT;
	}
	
	if(!Memory::CheckUsermodePointer((uintptr_t)msg->iov, sizeof(iovec) * msg->iovlen, proc->addressSpace)){
		Log::Warning("sys_sendmsg: msg: Invalid iovec ptr");
		return -EFAULT;
	}
	
	if(msg->name && !Memory::CheckUsermodePointer((uintptr_t)msg->name, msg->namelen, proc->addressSpace)){
		Log::Warning("sys_sendmsg: msg: Invalid name ptr and name not null");
		return -EFAULT;
	}

	long sent = 0;
	Socket* sock = (Socket*)handle->node;

	for(unsigned i = 0; i < msg->iovlen; i++){
		if(!Memory::CheckUsermodePointer((uintptr_t)msg->iov[i].base, msg->iov[i].len, proc->addressSpace)){
			Log::Warning("sys_sendmsg: msg: Invalid iovec entry base");
			return -EFAULT;
		}

		long ret = sock->SendTo(msg->iov[i].base, msg->iov[i].len, flags, (sockaddr*)msg->name, msg->namelen);

		if(ret < 0) return ret;

		sent += ret;
	}

	return sent;
}

/* 
 * SysRecvMsg (sockfd, msg, flags) - Recieve data through socket
 * sockfd - Socket file descriptor
 * msg - Message Header
 * flags - flags
 *
 * On Success - return amount of data received
 * On Failure - return -1
 */
long SysRecvMsg(regs64_t* r){
	process_t* proc = Scheduler::GetCurrentProcess();

	if(r->rbx >= proc->fileDescriptors.get_length()){
		Log::Warning("sys_sendmsg: Invalid File Descriptor: %d", r->rbx);
		return -1;
	}

	fs_fd_t* handle = proc->fileDescriptors.get_at(r->rbx);

	msghdr* msg = (msghdr*)r->rcx;
	uint64_t flags = r->rsi;

	if(!handle){ 
		Log::Warning("sys_recvmsg: Invalid file descriptor: ", r->rbx);
		return -1; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_recvmsg: File (Descriptor: %d) is not a socket", r->rbx);
		return -2;
	}
	
	if(!Memory::CheckUsermodePointer(r->rcx, sizeof(msghdr), proc->addressSpace)){
		Log::Warning("sys_recvmsg: Invalid msg ptr");
		return -EFAULT;
	}
	
	if(!Memory::CheckUsermodePointer((uintptr_t)msg->iov, sizeof(iovec) * msg->iovlen, proc->addressSpace)){
		Log::Warning("sys_recvmsg: msg: Invalid iovec ptr");
		return -EFAULT;
	}

	if(msg->name){
		Log::Warning("sys_recvmsg: msg: name ignored");
	}

	long read = 0;
	Socket* sock = (Socket*)handle->node;

	for(unsigned i = 0; i < msg->iovlen; i++){
		if(!Memory::CheckUsermodePointer((uintptr_t)msg->iov[i].base, msg->iov[i].len, proc->addressSpace)){
			Log::Warning("sys_recvmsg: msg: Invalid iovec entry base");
			return -EFAULT;
		}

		long ret = sock->Receive(msg->iov[i].base, msg->iov[i].len, flags);

		if(ret < 0) {
			return ret;
		}

		read += ret;
	}

	return read;
}

/* 
 * SysGetEUID () - Get effective process user id
 * 
 * On Success - Return process UID
 * On Failure - Does not fail
 */
long SysGetEUID(regs64_t* r){
	return Scheduler::GetCurrentProcess()->uid;
}

/* 
 * SysSetEUID () - Set effective process UID
 * 
 * On Success - Return 0
 * On Failure - Return negative value
 */
long SysSetEUID(regs64_t* r){
	return -ENOSYS;
}

/*
 * SysGetProcessInfo (pid, pInfo)
 * 
 * pid - Process PID
 * pInfo - Pointer to process_info_t structure
 * 
 * On Success - Return 0
 * On Failure - Return error as negative value
 */
long SysGetProcessInfo(regs64_t* r){
	uint64_t pid = r->rbx;
	process_info_t* pInfo = reinterpret_cast<process_info_t*>(r->rcx);

	process_t* cProcess = Scheduler::GetCurrentProcess();
	if(!Memory::CheckUsermodePointer(r->rcx, sizeof(process_info_t), cProcess->addressSpace)){
		return -EFAULT;
	}

	process_t* reqProcess;
	if(!(reqProcess = Scheduler::FindProcessByPID(pid))){
		return -EINVAL;
	}

	pInfo->pid = pid;

	pInfo->threadCount = reqProcess->threadCount;

	pInfo->uid = reqProcess->uid;
	pInfo->gid = reqProcess->gid;

	pInfo->state = reqProcess->state;

	strcpy(pInfo->name, reqProcess->name);

	pInfo->runningTime = Timer::GetSystemUptime() - reqProcess->creationTime.seconds;

	return 0;
}

/*
 * SysGetNextProcessInfo (pidP, pInfo)
 * 
 * pidP - Pointer to an unsigned integer holding a PID
 * pInfo - Pointer to process_info_t struct
 * 
 * On Success - Return 0
 * No more processes - Return 1
 * On Failure - Return error as negative value
 */
long SysGetNextProcessInfo(regs64_t* r){
	uint64_t* pidP = reinterpret_cast<uint64_t*>(r->rbx);
	process_info_t* pInfo = reinterpret_cast<process_info_t*>(r->rcx);

	process_t* cProcess = Scheduler::GetCurrentProcess();
	if(!Memory::CheckUsermodePointer(r->rcx, sizeof(process_info_t), cProcess->addressSpace)){
		return -EFAULT;
	}

	if(!Memory::CheckUsermodePointer(r->rbx, sizeof(uint64_t), cProcess->addressSpace)){
		return -EFAULT;
	}

	*pidP = Scheduler::GetNextProccessPID(*pidP);

	if(!(*pidP)){
		return 1; // No more processes
	}

	process_t* reqProcess;
	if(!(reqProcess = Scheduler::FindProcessByPID(*pidP))){
		return -EINVAL;
	}

	pInfo->pid = *pidP;

	pInfo->threadCount = reqProcess->threadCount;

	pInfo->uid = reqProcess->uid;
	pInfo->gid = reqProcess->gid;

	pInfo->state = reqProcess->state;

	strcpy(pInfo->name, reqProcess->name);

	pInfo->runningTime = Timer::GetSystemUptime() - reqProcess->creationTime.seconds;
	pInfo->activeUs = reqProcess->activeTicks * 1000000 / Timer::GetFrequency();

	return 0;
}

syscall_t syscalls[]{
	SysDebug,
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
	SysFStat,
	SysStat,
	SysLSeek,
	SysGetPID,					// 20
	SysMount,
	SysMkdir,
	SysRmdir,
	SysRename,
	SysYield,					// 25
	SysReadDirNext,
	SysRenameAt,
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
	SysIoctl,
	SysInfo,
	SysMunmap,
	SysCreateSharedMemory,		// 45
	SysMapSharedMemory,
	SysUnmapSharedMemory,
	SysDestroySharedMemory,
	SysSocket,
	SysBind,					// 50
	SysListen,
	SysAccept,
	SysConnect,
	SysSend,
	SysSendTo,					// 55
	SysReceive,
	SysReceiveFrom,
	SysGetUID,
	SysSetUID,					
	SysPoll,					// 60
	SysSendMsg,
	SysRecvMsg,
	SysGetEUID,
	SysSetEUID,
	SysGetProcessInfo,			// 65
	SysGetNextProcessInfo,
};

int lastSyscall = 0;
void SyscallHandler(regs64_t* regs) {
	if (regs->rax >= NUM_SYSCALLS) // If syscall is non-existant then return
		return;
		
	asm("sti");

	if(!syscalls[regs->rax]) return;

	acquireLock(&GetCPULocal()->currentThread->lock);

	regs->rax = syscalls[regs->rax](regs); // Call syscall

	releaseLock(&GetCPULocal()->currentThread->lock);
}

void InitializeSyscalls() {
	IDT::RegisterInterruptHandler(0x69, SyscallHandler);
}