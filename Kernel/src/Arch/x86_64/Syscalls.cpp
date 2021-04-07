#include <Syscalls.h>

#include <Errno.h>
#include <IDT.h>
#include <Scheduler.h>
#include <Logging.h>
#include <Video/Video.h>
#include <HAL.h>
#include <Framebuffer.h>
#include <PhysicalAllocator.h>
#include <Timer.h>
#include <PTY.h>
#include <Lock.h>
#include <Lemon.h>
#include <SharedMemory.h>
#include <CPU.h>
#include <Net/Socket.h>
#include <Timer.h>
#include <Lock.h>
#include <SMP.h>
#include <Pair.h>
#include <Objects/Service.h>
#include <Debug.h>
#include <StackTrace.h>
#include <Math.h>
#include <Device.h>
#include <Modules.h>

#include <ABI/Process.h>
#include <ABI/Syscall.h>

#define SC_ARG0(r) (r)->rdi
#define SC_ARG1(r) (r)->rsi
#define SC_ARG2(r) (r)->rdx
#define SC_ARG3(r) (r)->r10
#define SC_ARG4(r) (r)->r9
#define SC_ARG5(r) (r)->r8

#define NUM_SYSCALLS 93

#define EXEC_CHILD 1

#define WCONTINUED 1
#define WNOHANG 2
#define WUNTRACED 4
#define WEXITED 8
#define WNOWAIT 16
#define WSTOPPED 32

typedef long(*syscall_t)(RegisterContext*);

long SysExit(RegisterContext* r){
	int64_t code = SC_ARG0(r);

	Log::Info("Process %s (PID: %d) exiting with code %d", Scheduler::GetCurrentProcess()->name, Scheduler::GetCurrentProcess()->pid, code);

	IF_DEBUG(debugLevelSyscalls >= DebugLevelVerbose, {
		Log::Info("rip: %x", r->rip);
		UserPrintStackTrace(r->rbp, Scheduler::GetCurrentProcess()->addressSpace);
	});

	Scheduler::EndProcess(Scheduler::GetCurrentProcess());
	return 0;
}

long SysExec(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	size_t filePathLength;
	long filePathInvalid = strlenSafe(reinterpret_cast<char*>(SC_ARG0(r)), filePathLength, currentProcess->addressSpace);
	if(filePathInvalid){
		Log::Warning("SysExec: Reached unallocated memory reading file path");
		return -EFAULT;
	}

	char filepath[filePathLength + 1];

	strncpy(filepath, (char*)SC_ARG0(r), filePathLength);
	int argc = SC_ARG1(r);
	char** argv = (char**)SC_ARG2(r);
	uint64_t flags = SC_ARG3(r);
	char** envp = (char**)SC_ARG4(r);

	FsNode* node = fs::ResolvePath(filepath, currentProcess->workingDir, true /* Follow Symlinks */);
	if(!node){
		return -ENOENT;
	}
	
	int envCount = 0;
	if(envp){
		int i = 0;
        while(envp[i]) i++;
		envCount = i;
	}

	char* kernelEnvp[envCount];
	if(envCount > 0){
		for(int i = 0; i < envCount; i++){
			size_t len;
			if(strlenSafe(envp[i], len, currentProcess->addressSpace)){
				Log::Warning("SysExec: Reached unallocated memory reading environment");
				return -EFAULT;
			}
			kernelEnvp[i] = (char*)kmalloc(len + 1);
			strcpy(kernelEnvp[i], envp[i]);
			kernelEnvp[i][len] = 0;
		}
	}

	char* kernelArgv[argc];
	for(int i = 0; i < argc; i++){
		kernelArgv[i] = (char*)kmalloc(strlen(argv[i]) + 1);
		strcpy(kernelArgv[i], argv[i]);
	}

	Log::Info("Loading: %s", (char*)SC_ARG0(r));
	timeval tv = Timer::GetSystemUptimeStruct();
	uint8_t* buffer = (uint8_t*)kmalloc(node->size);
	size_t read = fs::Read(node, 0, node->size, buffer);
	if(!read){
		Log::Warning("Could not read file: %s", filepath);
		return 0;
	}
	timeval tvnew = Timer::GetSystemUptimeStruct();
	Log::Info("Done (took %d us)", Timer::TimeDifference(tvnew, tv));

	process_t* proc = Scheduler::CreateELFProcess((void*)buffer, argc, kernelArgv, envCount, kernelEnvp, filepath);
	kfree(buffer);
	
	for(int i = 0; i < envCount; i++){
		kfree(kernelEnvp[i]);
	}

	if(!proc) {
		for(int i = 0; i < argc; i++){
			kfree(kernelArgv[i]);
		}

		return -1;
	}

	for(int i = 0; i < argc; i++){
		kfree(kernelArgv[i]);
	}

	if(!proc){
		return -1; // Failed to create process
	}

	// TODO: Do not run process until we have finished

	if(flags & EXEC_CHILD){
		currentProcess->children.add_back(proc);
		proc->parent = Scheduler::GetCurrentProcess();

		proc->fileDescriptors[0] = new fs_fd_t(*Scheduler::GetCurrentProcess()->fileDescriptors.get_at(0));
		proc->fileDescriptors[1] = new fs_fd_t(*Scheduler::GetCurrentProcess()->fileDescriptors.get_at(1));
		proc->fileDescriptors[2] = new fs_fd_t(*Scheduler::GetCurrentProcess()->fileDescriptors.get_at(2));
	}

	strncpy(proc->workingDir, Scheduler::GetCurrentProcess()->workingDir, PATH_MAX);

	char* name;
	if(argc > 0){
		name = fs::BaseName(kernelArgv[0]);
	} else {
		name = fs::BaseName(filepath);
	}
	strncpy(proc->name, name, NAME_MAX);

	kfree(name);

	return proc->pid;
}

long SysRead(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	if(SC_ARG0(r) >= proc->fileDescriptors.get_length()){
		Log::Warning("Invalid File Descriptor: %d", SC_ARG0(r));
		return -EBADF;
	}
	fs_fd_t* handle = proc->fileDescriptors[SC_ARG0(r)];
	if(!handle){
		Log::Warning("Invalid File Descriptor: %d", SC_ARG0(r));
		return -EBADF;
	}

	uint8_t* buffer = (uint8_t*)SC_ARG1(r);
	uint64_t count = SC_ARG2(r);

	if(!Memory::CheckUsermodePointer(SC_ARG1(r), count, proc->addressSpace)){
		Log::Warning("SysRead: Invalid Memory Buffer: %x", SC_ARG1(r));
		return -EFAULT;
	}

	ssize_t ret = fs::Read(handle, count, buffer);
	return ret;
}

long SysWrite(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();

	if(SC_ARG0(r) >= proc->fileDescriptors.get_length()){
		Log::Warning("Invalid File Descriptor: %d", SC_ARG0(r));
		return -EBADF;
	}
	fs_fd_t* handle = proc->fileDescriptors[SC_ARG0(r)];
	if(!handle){
		Log::Warning("Invalid File Descriptor: %d", SC_ARG0(r));
		return -EBADF;
	}

	uint8_t* buffer = (uint8_t*)SC_ARG1(r);
	uint64_t count = SC_ARG2(r);

	if(!Memory::CheckUsermodePointer(SC_ARG1(r), count, proc->addressSpace)){
		Log::Warning("SysWrite: Invalid Memory Buffer: %x", SC_ARG1(r));
		return -EFAULT;
	}

	ssize_t ret = fs::Write(handle, SC_ARG2(r), buffer);
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
long SysOpen(RegisterContext* r){
	char* filepath = (char*)kmalloc(strlen((char*)SC_ARG0(r)) + 1);
	strcpy(filepath, (char*)SC_ARG0(r));
	FsNode* root = fs::GetRoot();

	uint64_t flags = SC_ARG1(r);

	process_t* proc = Scheduler::GetCurrentProcess();

	IF_DEBUG(debugLevelSyscalls >= DebugLevelVerbose, {
		Log::Info("Opening: %s (flags: %u)", filepath, flags);
	});
	
	long fd;
	if(strcmp(filepath,"/") == 0){
		fd = proc->fileDescriptors.get_length();
		proc->fileDescriptors.add_back(fs::Open(root, 0));
		return fd;
	}

open:
	FsNode* node = fs::ResolvePath(filepath, proc->workingDir, !(flags & O_NOFOLLOW));

	if(!node){
		if(flags & O_CREAT){
			FsNode* parent = fs::ResolveParent(filepath, proc->workingDir);
			char* basename = fs::BaseName(filepath);

			if(!parent) {
				Log::Warning("SysOpen: Could not resolve parent directory of new file %s", basename);
				return -ENOENT;
			}

			DirectoryEntry ent;
			strcpy(ent.name, basename);

			IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
				Log::Info("SysOpen: Creating %s", filepath);
			});

			parent->Create(&ent, flags);

			kfree(basename);

			flags &= ~O_CREAT;
			goto open;
		} else {
			IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
				Log::Warning("SysOpen (flags %x): Failed to open file %s", flags, filepath);
			});
			return -ENOENT;
		}
	}

	if(flags & O_DIRECTORY && ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY)){
		return -ENOTDIR;
	}

	if(flags & O_TRUNC && ((flags & O_ACCESS) == O_RDWR || (flags & O_ACCESS) == O_WRONLY)){
		node->Truncate(0);
	}

	fs_fd_t* handle = fs::Open(node, SC_ARG1(r));

	if(!handle){
		Log::Warning("SysOpen: Error retrieving file handle for node. Dangling symlink?");
		return -ENOENT;
	}

	if(flags & O_APPEND){
		handle->pos = handle->node->size;
	}

	fd = Scheduler::GetCurrentProcess()->fileDescriptors.get_length();
	Scheduler::GetCurrentProcess()->fileDescriptors.add_back(handle);
	fs::Open(node, flags);

	return fd;
}

long SysClose(RegisterContext* r){
	int fd = SC_ARG0(r);
	
	process_t* currentProcess = Scheduler::GetCurrentProcess();
	if(fd >= static_cast<int>(currentProcess->fileDescriptors.get_length())){
		Log::Warning("SysClose: Invalid File Descriptor, %d", fd);
		return -EBADF;
	}

	fs_fd_t* handle;
	if((handle = currentProcess->fileDescriptors.at(fd))){
		fs::Close(handle);

		handle->node = nullptr;

		Scheduler::GetCurrentProcess()->fileDescriptors.at(fd) = nullptr;
	}
	return 0;
}

long SysSleep(RegisterContext* r){
	return 0;
}

long SysCreate(RegisterContext* r){
	return 0;
}

long SysLink(RegisterContext* r){
	const char* oldpath = (const char*)SC_ARG0(r);
	const char* newpath = (const char*)SC_ARG1(r);

	process_t* proc = Scheduler::GetCurrentProcess();

	if(!(Memory::CheckUsermodePointer(SC_ARG0(r), 1, proc->addressSpace) && Memory::CheckUsermodePointer(SC_ARG1(r), 1, proc->addressSpace))){
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

long SysUnlink(RegisterContext* r){
	const char* path = (const char*)SC_ARG0(r);
	
	process_t* proc = Scheduler::GetCurrentProcess();

	if(!Memory::CheckUsermodePointer(SC_ARG0(r), 1, proc->addressSpace)){
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

long SysChdir(RegisterContext* r){
	if(SC_ARG0(r)){
		char* path =  fs::CanonicalizePath((char*)SC_ARG0(r), Scheduler::GetCurrentProcess()->workingDir);
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

long SysTime(RegisterContext* r){
	return 0;
}

long SysMapFB(RegisterContext *r){
	video_mode_t vMode = Video::GetVideoMode();

	uint64_t pageCount = (vMode.height * vMode.pitch + 0xFFF) >> 12;
	uintptr_t fbVirt = (uintptr_t)Memory::Allocate4KPages(pageCount, Scheduler::GetCurrentProcess()->addressSpace);
	Memory::MapVirtualMemory4K((uintptr_t)HAL::videoMode.physicalAddress,fbVirt,pageCount,Scheduler::GetCurrentProcess()->addressSpace);

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

	*((uintptr_t*)SC_ARG0(r)) = fbVirt;
	*((fb_info_t*)SC_ARG1(r)) = fbInfo;
	
	return 0;
}

long SysAlloc(RegisterContext* r){
	uint64_t pageCount = SC_ARG0(r);
	uintptr_t* addressPointer = (uintptr_t*)SC_ARG1(r);

	process_t* proc = Scheduler::GetCurrentProcess();
	uintptr_t address = (uintptr_t)Memory::Allocate4KPages(pageCount, proc->addressSpace);

	assert(address);

	for(unsigned i = 0; i < pageCount; i++){
		Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), address + i * PAGE_SIZE_4K, 1, proc->addressSpace);
		memset((void*)(address + i * PAGE_SIZE_4K), 0, PAGE_SIZE_4K);
	}

	*addressPointer = address;

	proc->usedMemoryBlocks += pageCount;
	return 0;
}

long SysChmod(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();

	const char* path = reinterpret_cast<const char*>(SC_ARG0(r));
	mode_t mode = SC_ARG1(r);

	size_t pathLen = 0;
	if(strlenSafe(path, pathLen, proc->addressSpace)){
		return -EFAULT;
	}

	if(pathLen > PATH_MAX){
		return -ENAMETOOLONG;
	}

	char tempPath[pathLen + 1];
	strcpy(tempPath, path);

	FsNode* file = fs::ResolvePath(tempPath, proc->workingDir);
	if(!file){
		return -ENOENT;
	}

	Log::Warning("SysChmod: chmod is a stub!");

	(void)mode;

	return 0;
}

long SysFStat(RegisterContext* r){
	stat_t* stat = (stat_t*)SC_ARG0(r);
	int fd = SC_ARG1(r);

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

long SysStat(RegisterContext* r){
	stat_t* stat = (stat_t*)SC_ARG0(r);
	char* filepath = (char*)SC_ARG1(r);
	uint64_t flags = SC_ARG2(r);
	process_t* proc = Scheduler::GetCurrentProcess();

	if(!Memory::CheckUsermodePointer(SC_ARG0(r), sizeof(stat_t), proc->addressSpace)){
		Log::Warning("sys_stat: stat structure points to invalid address %x", SC_ARG0(r));
	}
	
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), 1, proc->addressSpace)){
		Log::Warning("sys_stat: filepath points to invalid address %x", SC_ARG0(r));
	}

	bool followSymlinks = !(flags & AT_SYMLINK_NOFOLLOW);
	FsNode* node = fs::ResolvePath(filepath, proc->workingDir, followSymlinks);

	if(!node){
		Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "sys_stat: Invalid filepath %s", filepath);
		return -ENOENT;
	}

	stat->st_dev = 0;
	stat->st_ino = node->inode;
	stat->st_mode = 0;
	
	if((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY) stat->st_mode |= S_IFDIR;
	if((node->flags & FS_NODE_TYPE) == FS_NODE_FILE) stat->st_mode |= S_IFREG;
	if((node->flags & FS_NODE_TYPE) == FS_NODE_BLKDEVICE) stat->st_mode |= S_IFBLK;
	if((node->flags & FS_NODE_TYPE) == FS_NODE_CHARDEVICE) stat->st_mode |= S_IFCHR;
	if((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK) stat->st_mode |= S_IFLNK;
	if((node->flags & FS_NODE_TYPE) == FS_NODE_SOCKET) stat->st_mode |= S_IFSOCK;

	stat->st_nlink = 0;
	stat->st_uid = node->uid;
	stat->st_gid = 0;
	stat->st_rdev = 0;
	stat->st_size = node->size;
	stat->st_blksize = 0;
	stat->st_blocks = 0;

	return 0;
}

long SysLSeek(RegisterContext* r){
	long ret = 0;
	int fd = SC_ARG0(r);

	if(fd >= static_cast<int>(Scheduler::GetCurrentProcess()->fileDescriptors.get_length()) || !Scheduler::GetCurrentProcess()->fileDescriptors[fd]){
		Log::Warning("sys_lseek: Invalid File Descriptor, %d", fd);
		return -EINVAL;
	}

	switch(SC_ARG2(r)){
	case 0: // SEEK_SET
		ret = Scheduler::GetCurrentProcess()->fileDescriptors[fd]->pos = SC_ARG1(r);
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
		Log::Info("Invalid seek: %d, mode: %d", fd, SC_ARG2(r));
		return -EINVAL; // Invalid seek mode
		break;
	}

	return ret;
}

long SysGetPID(RegisterContext* r){
	uint64_t* pid = (uint64_t*)SC_ARG0(r);

	*pid = Scheduler::GetCurrentProcess()->pid;
	
	return 0;
}

long SysMount(RegisterContext* r){
	return 0;
}

long SysMkdir(RegisterContext* r){
	char* path = (char*)SC_ARG0(r);
	mode_t mode = SC_ARG1(r);

	process_t* proc = Scheduler::GetCurrentProcess();

	if(!Memory::CheckUsermodePointer(SC_ARG0(r), 0, proc->addressSpace)){
		Log::Warning("sys_mkdir: Invalid path pointer %x", SC_ARG0(r));
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

long SysRmdir(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	if(!Memory::CheckUsermodePointer(SC_ARG0(r), 1, proc->addressSpace)){
		return -EFAULT;
	}

	char* path = reinterpret_cast<char*>(SC_ARG0(r));
	FsNode* node = fs::ResolvePath(path, proc->workingDir);
	if(!node){
		return -ENOENT;
	}

	if((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
		return -ENOTDIR;
	}

	DirectoryEntry ent;
	if(node->ReadDir(&ent, 0)){
		return -ENOTEMPTY;
	}

	FsNode* parent = fs::ResolveParent(path);
	strcpy(ent.name, fs::BaseName(path));
	ent.inode = node->inode;

	fs::Unlink(parent, &ent, true);
	return 0;
}

long SysRename(RegisterContext* r){
	char* oldpath = (char*)SC_ARG0(r);
	char* newpath = (char*)SC_ARG1(r);

	process_t* proc = Scheduler::GetCurrentProcess();

	if(!Memory::CheckUsermodePointer(SC_ARG0(r), 0, proc->addressSpace)){
		Log::Warning("sys_rename: Invalid oldpath pointer %x", SC_ARG0(r));
		return -EFAULT;
	}
	
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), 0, proc->addressSpace)){
		Log::Warning("sys_rename: Invalid newpath pointer %x", SC_ARG0(r));
		return -EFAULT;
	}

	FsNode* olddir = fs::ResolveParent(oldpath, proc->workingDir);
	FsNode* newdir = fs::ResolveParent(newpath, proc->workingDir);

	if(!(olddir && newdir)){
		return -ENOENT;
	}

	return fs::Rename(olddir, fs::BaseName(oldpath), newdir, fs::BaseName(newpath));
}

long SysYield(RegisterContext* r){
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
long SysReadDirNext(RegisterContext* r){
	process_t* process = Scheduler::GetCurrentProcess();

	unsigned int fd = SC_ARG0(r);
	if(fd > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
		return -EBADF;
	}
	
	fs_dirent_t* direntPointer = (fs_dirent_t*)SC_ARG1(r);

	fs_fd_t* handle = process->fileDescriptors[fd];
	if(!handle){
		return -EBADF;
	}

	if(!Memory::CheckUsermodePointer((uintptr_t)direntPointer, sizeof(fs_dirent_t), Scheduler::GetCurrentProcess()->addressSpace)){
		return -EFAULT;
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
		return -ENOTDIR;
	}

	DirectoryEntry tempent;
	int ret = fs::ReadDir(handle, &tempent, handle->pos++);

	strcpy(direntPointer->name, tempent.name);
	direntPointer->type = tempent.flags;

	return ret;
}

long SysRenameAt(RegisterContext* r){
	Log::Warning("SysRenameAt is a stub!");
	return -ENOSYS;
}

// SendMessage(message_t* msg) - Sends an IPC message to a process
long SysSendMessage(RegisterContext* r){
	return -ENOSYS;
}

// RecieveMessage(message_t* msg) - Grabs next message on queue and copies it to msg
long SysReceiveMessage(RegisterContext* r){
	return -ENOSYS;
}

long SysUptime(RegisterContext* r){
	uint64_t* seconds = (uint64_t*)SC_ARG0(r);
	uint64_t* milliseconds = (uint64_t*)SC_ARG1(r);
	if(seconds){
		*seconds = Timer::GetSystemUptime();
	}
	if(milliseconds){
		*milliseconds = Timer::GetTicks() * 1000 / Timer::GetFrequency();
	}
	return 0;
}

long SysDebug(RegisterContext* r){
	Log::Info("(%s): %s, %d", Scheduler::GetCurrentProcess()->name, (char*)SC_ARG0(r), SC_ARG1(r));
	return 0;
}

long SysGetVideoMode(RegisterContext* r){
	video_mode_t vMode = Video::GetVideoMode();
	fb_info_t fbInfo;
	fbInfo.width = vMode.width;
	fbInfo.height = vMode.height;
	if(HAL::debugMode) fbInfo.height = vMode.height / 3 * 2;
	fbInfo.bpp = vMode.bpp;
	fbInfo.pitch = vMode.pitch;

	*((fb_info_t*)SC_ARG0(r)) = fbInfo;

	return 0;
}

long SysUName(RegisterContext* r){
	if(Memory::CheckUsermodePointer(SC_ARG0(r), strlen(Lemon::versionString), Scheduler::GetCurrentProcess()->addressSpace)) {
		strcpy((char*) SC_ARG0(r), Lemon::versionString);
		return 0;
	}

	return -EFAULT;
}

long SysReadDir(RegisterContext* r){
	int fd = SC_ARG0(r);

	if(fd >= static_cast<int>(Scheduler::GetCurrentProcess()->fileDescriptors.get_length())){
		return -EBADF;
	} 
	
	fs_dirent_t* direntPointer = (fs_dirent_t*)SC_ARG1(r);

	if(!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(fs_dirent_t), Scheduler::GetCurrentProcess()->addressSpace)){
		return -EFAULT;
	}

	unsigned int count = SC_ARG2(r);

	if((Scheduler::GetCurrentProcess()->fileDescriptors[fd]->node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
		return -ENOTDIR;
	}

	DirectoryEntry tempent;
	int ret = fs::ReadDir(Scheduler::GetCurrentProcess()->fileDescriptors[fd], &tempent, count);

	strcpy(direntPointer->name, tempent.name);
	direntPointer->type = tempent.flags;

	return ret;
}

long SysSetFsBase(RegisterContext* r){
	asm volatile ("wrmsr" :: "a"(SC_ARG0(r) & 0xFFFFFFFF) /*Value low*/, "d"((SC_ARG0(r) >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000100) /*Set FS Base*/);
	GetCPULocal()->currentThread->fsBase = SC_ARG0(r);
	return 0;
}

long SysMmap(RegisterContext* r){
	uint64_t* address = (uint64_t*)SC_ARG0(r);
	size_t count = SC_ARG1(r);
	uintptr_t hint = SC_ARG2(r);

	process_t* proc = Scheduler::GetCurrentProcess();
	uintptr_t _address;
	if(hint){
		if(Memory::CheckRegion(hint, count * PAGE_SIZE_4K, proc->addressSpace) /*Check availibilty of the requested map*/){
			_address = hint;
		} else {
			Log::Warning("sys_mmap: Could not map to address %x", hint);
			*address = 0;
			return 1;
		}
	} else _address = (uintptr_t)Memory::Allocate4KPages(count, proc->addressSpace);

	for(size_t i = 0; i < count; i++){
		Memory::MapVirtualMemory4K(Memory::AllocatePhysicalMemoryBlock(), _address + i * PAGE_SIZE_4K, 1, proc->addressSpace);
		memset((void*)(_address + i * PAGE_SIZE_4K), 0, PAGE_SIZE_4K);
	}

	*address = _address;

	proc->usedMemoryBlocks += count;
	return 0;
}

long SysGrantPTY(RegisterContext* r){
	if(!SC_ARG0(r)) return 1;

	PTY* pty = GrantPTY(Scheduler::GetCurrentProcess()->pid);

	process_t* currentProcess = Scheduler::GetCurrentProcess();

	*((int*)SC_ARG0(r)) = currentProcess->fileDescriptors.get_length();
	
	currentProcess->fileDescriptors[0] = fs::Open(&pty->slaveFile); // Stdin
	currentProcess->fileDescriptors[1] = fs::Open(&pty->slaveFile); // Stdout
	currentProcess->fileDescriptors[2] = fs::Open(&pty->slaveFile); // Stderr

	currentProcess->fileDescriptors.add_back(fs::Open(&pty->masterFile));

	return 0;
}

long SysGetCWD(RegisterContext* r){
	char* buf = (char*)SC_ARG0(r);
	size_t sz = SC_ARG1(r);

	char* workingDir = Scheduler::GetCurrentProcess()->workingDir;
	if(strlen(workingDir) > sz) {
		return 1;
	} else {
		strcpy(buf, workingDir);
		return 0;
	}

	return 0;
}

long SysWaitPID(RegisterContext* r){
	int64_t pid = SC_ARG0(r);
	int64_t flags = SC_ARG2(r);

	process_t* proc = nullptr;
	if((proc = Scheduler::FindProcessByPID(pid))){
		if(flags & WNOHANG){
			return 0;
		}

		int pid = 0;
		Scheduler::ProcessStateThreadBlocker bl = Scheduler::ProcessStateThreadBlocker(pid);

		bl.WaitOn(proc);

		if(Scheduler::GetCurrentThread()->Block(&bl)){
			return -EINTR;
		}
	}

	if((proc = Scheduler::FindProcessByPID(pid))) {
		return -1;
	}

	return pid;
}

long SysNanoSleep(RegisterContext* r){
	uint64_t nanoseconds = SC_ARG0(r);

	Scheduler::GetCurrentThread()->Sleep(nanoseconds / 1000);

	return 0;
}

long SysPRead(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();
	if(SC_ARG0(r) >= currentProcess->fileDescriptors.get_length()){
		return -EBADF;
	}

	FsNode* node;
	if(!currentProcess->fileDescriptors.get_at(SC_ARG0(r)) || !(node = currentProcess->fileDescriptors.get_at(SC_ARG0(r))->node)){ 
		IF_DEBUG(debugLevelSyscalls >= DebugLevelVerbose, {
			Log::Warning("sys_pread: Invalid file descriptor: %d", SC_ARG0(r));
			
		});
		return -EBADF; 
	}
	
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), SC_ARG2(r), Scheduler::GetCurrentProcess()->addressSpace)) {
		return -EFAULT;
	}

	uint8_t* buffer = (uint8_t*)SC_ARG1(r);
	uint64_t count = SC_ARG2(r);
	uint64_t off = SC_ARG4(r);
	return fs::Read(node, off, count, buffer);
}

long SysPWrite(RegisterContext* r){
	if(SC_ARG0(r) > Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
		return -EBADF;
	}
	FsNode* node;
	if(Scheduler::GetCurrentProcess()->fileDescriptors.get_at(SC_ARG0(r)) || !(node = Scheduler::GetCurrentProcess()->fileDescriptors.get_at(SC_ARG0(r))->node)){ 
		Log::Warning("sys_pwrite: Invalid file descriptor: %d", SC_ARG0(r));
		return -EBADF;
	}

	if(!Memory::CheckUsermodePointer(SC_ARG1(r), SC_ARG2(r), Scheduler::GetCurrentProcess()->addressSpace)) {
		return -EFAULT;
	}

	uint64_t off = SC_ARG4(r);

	return fs::Write(node, off, SC_ARG2(r), (uint8_t*)SC_ARG1(r));
}

long SysIoctl(RegisterContext* r){
	int fd = SC_ARG0(r);
	uint64_t request = SC_ARG1(r);
	uint64_t arg = SC_ARG2(r);
	int* result = (int*)SC_ARG3(r);

	if(result && !Memory::CheckUsermodePointer((uintptr_t)result, sizeof(int), Scheduler::GetCurrentProcess()->addressSpace)){
		return -EFAULT;
	}

	if(fd >= static_cast<int>(Scheduler::GetCurrentProcess()->fileDescriptors.get_length())){
		return -EINVAL;
	}
	fs_fd_t* handle = Scheduler::GetCurrentProcess()->fileDescriptors[SC_ARG0(r)];
	if(!handle){
		Log::Warning("sys_ioctl: Invalid File Descriptor: %d", SC_ARG0(r));
		return -EINVAL;
	}

	int ret = fs::Ioctl(handle, request, arg);

	if(result && ret > 0){
		*result = ret;
	}

	return ret;
}

long SysInfo(RegisterContext* r){
	lemon_sysinfo_t* s = (lemon_sysinfo_t*)SC_ARG0(r);

	if(!s){
		return -EFAULT;
	}

	s->usedMem = Memory::usedPhysicalBlocks * 4;
	s->totalMem = HAL::mem_info.totalMemory / 1024;
	s->cpuCount = static_cast<uint16_t>(SMP::processorCount);

	return 0;
}

/*
 * SysMunmap - Unmap memory (addr, count)
 * 
 * On success - return 0
 * On failure - return -1
 */
long SysMunmap(RegisterContext* r){
	process_t* process = Scheduler::GetCurrentProcess();

	uint64_t address = SC_ARG0(r);
	size_t count = SC_ARG1(r);

	uint64_t endAddress = address + count * PAGE_SIZE_4K;

	if(Memory::CheckRegion(address, endAddress, Scheduler::GetCurrentProcess()->addressSpace) /*Check availibilty of the requested map*/){
		for(mem_region_t& shRegion : process->sharedMemory){
			if((address >= shRegion.base && address <= shRegion.base + shRegion.pageCount * PAGE_SIZE_4K) || (endAddress >= shRegion.base && address <= shRegion.base + shRegion.pageCount * PAGE_SIZE_4K)){
				return -EINVAL;
			} else if((shRegion.base >= address && shRegion.base <= endAddress) || (shRegion.base + shRegion.pageCount * PAGE_SIZE_4K >= address && shRegion.base + shRegion.pageCount * PAGE_SIZE_4K <= endAddress)){
				return -EINVAL; // Check for overlap with shared memory
			} // TODO: Unmap around shared memory, maybe keep track of all memory regions, not just shared
		}

		for(unsigned i = 0; i < count; i++){
			if(uintptr_t mem = Memory::VirtualToPhysicalAddress(address + i * PAGE_SIZE_4K, process->addressSpace); mem){
				Memory::FreePhysicalMemoryBlock(mem);
				Log::Info("freeing mem %d", mem);
			}
		}

		Memory::Free4KPages((void*)address, count, Scheduler::GetCurrentProcess()->addressSpace);
	} else {
		return -EFAULT;
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
long SysCreateSharedMemory(RegisterContext* r){
	int64_t* key = (int64_t*)SC_ARG0(r);
	uint64_t size = SC_ARG1(r);
	uint64_t flags = SC_ARG2(r);
	uint64_t recipient = SC_ARG3(r);

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
long SysMapSharedMemory(RegisterContext* r){
	void** ptr = (void**)SC_ARG0(r);
	int64_t key = SC_ARG1(r);
	uint64_t hint = SC_ARG2(r);

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
long SysUnmapSharedMemory(RegisterContext* r){
	uint64_t address = SC_ARG0(r);
	int64_t key = SC_ARG1(r);

	shared_mem_t* sMem = Memory::GetSharedMemory(key);
	if(!sMem) return -EINVAL;

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
long SysDestroySharedMemory(RegisterContext* r){
	uint64_t key = SC_ARG0(r);

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
long SysSocket(RegisterContext* r){
	int domain = SC_ARG0(r);
	int type = SC_ARG1(r);
	int protocol = SC_ARG2(r);

	Socket* sock = nullptr;
	int e = Socket::CreateSocket(domain, type, protocol, &sock);

	if(e < 0) return e;
	assert(sock);
	
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
long SysBind(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	long fd = SC_ARG0(r);

	if(fd >= proc->fileDescriptors.get_length()){
		Log::Warning("SysBind: Invalid file descriptor: ", fd);
		return -EBADF; 
	}
	fs_fd_t* handle = proc->fileDescriptors.get_at(fd);
	if(!handle){ 
		Log::Warning("SysBind: Invalid file descriptor: ", fd);
		return -EBADF; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("SysBind: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		return -ENOTSOCK;
	}

	socklen_t len = SC_ARG2(r);
	
	sockaddr_t* addr = (sockaddr_t*)SC_ARG1(r);
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)){
		Log::Warning("SysBind: Invalid sockaddr ptr");
		return -EINVAL;
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
long SysListen(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(SC_ARG0(r));
	if(!handle){ 
		Log::Warning("sys_listen: Invalid file descriptor: ", SC_ARG0(r));
		return -EINVAL; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_listen: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		return -ENOTSOCK;
	}

	Socket* sock = (Socket*)handle->node;
	return sock->Listen(SC_ARG1(r));
}

/////////////////////////////
/// \name SysAccept (sockfd, addr, addrlen) - Accept socket connection
/// \param sockfd - Socket file descriptor
/// \param addr - sockaddr structure
/// \param addrlen - pointer to size of addr
///
/// \return File descriptor of accepted socket or negative error code
/////////////////////////////
long SysAccept(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(SC_ARG0(r));
	if(!handle){ 
		Log::Warning("sys_accept: Invalid file descriptor: ", SC_ARG0(r));
		return -EINVAL; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_accept: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		return -ENOTSOCK;
	}

	socklen_t* len = (socklen_t*)SC_ARG2(r);
	if(len && !Memory::CheckUsermodePointer(SC_ARG2(r), sizeof(socklen_t), proc->addressSpace)){
		Log::Warning("sys_accept: Invalid socklen ptr");
		return -EFAULT;
	}
	
	sockaddr_t* addr = (sockaddr_t*)SC_ARG1(r);
	if(addr && !Memory::CheckUsermodePointer(SC_ARG1(r), *len, proc->addressSpace)){
		Log::Warning("sys_accept: Invalid sockaddr ptr");
		return -EFAULT;
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

/////////////////////////////
/// \name SysConnect (sockfd, addr, addrlen) - Initiate socket connection
/// \param sockfd - Socket file descriptor
/// \param addr - sockaddr structure
/// \param addrlen - size of addr
///
/// \return 0 on success or negative error code on failure
/////////////////////////////
long SysConnect(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(SC_ARG0(r));
	if(!handle){ 
		Log::Warning("sys_connect: Invalid file descriptor: ", SC_ARG0(r));
		return -EINVAL; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_connect: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		return -EFAULT;
	}

	socklen_t len = SC_ARG2(r);
	
	sockaddr_t* addr = (sockaddr_t*)SC_ARG1(r);
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)){
		Log::Warning("sys_connect: Invalid sockaddr ptr");
		return -EFAULT;
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
long SysSend(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(SC_ARG0(r));

	uint8_t* buffer = (uint8_t*)(SC_ARG1(r));
	size_t len = SC_ARG2(r);
	uint64_t flags = SC_ARG3(r);

	if(!handle){ 
		Log::Warning("sys_send: Invalid file descriptor: ", SC_ARG0(r));
		return -EINVAL; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_send: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		return -ENOTSOCK;
	}
	
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)){
		Log::Warning("sys_send: Invalid buffer ptr");
		return -EFAULT;
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
long SysSendTo(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(SC_ARG0(r));

	uint8_t* buffer = (uint8_t*)(SC_ARG1(r));
	size_t len = SC_ARG2(r);
	uint64_t flags = SC_ARG3(r);

	if(!handle){ 
		Log::Warning("sys_send: Invalid file descriptor: ", SC_ARG0(r));
		return -EINVAL; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_send: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		return -ENOTSOCK;
	}
	
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)){
		Log::Warning("sys_send: Invalid buffer ptr");
		return -EFAULT;
	}
	
	socklen_t slen = SC_ARG2(r);
	
	sockaddr_t* addr = (sockaddr_t*)SC_ARG1(r);

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
long SysReceive(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(SC_ARG0(r));

	uint8_t* buffer = (uint8_t*)(SC_ARG1(r));
	size_t len = SC_ARG2(r);
	uint64_t flags = SC_ARG3(r);

	if(!handle){ 
		Log::Warning("sys_send: Invalid file descriptor: ", SC_ARG0(r));
		return -EINVAL; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_send: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		return -ENOTSOCK;
	}
	
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)){
		Log::Warning("sys_send: Invalid buffer ptr");
		return -EINVAL;
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
long SysReceiveFrom(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	fs_fd_t* handle = proc->fileDescriptors.get_at(SC_ARG0(r));

	uint8_t* buffer = (uint8_t*)(SC_ARG1(r));
	size_t len = SC_ARG2(r);
	uint64_t flags = SC_ARG3(r);

	if(!handle){ 
		Log::Warning("sys_send: Invalid file descriptor: ", SC_ARG0(r));
		return -EINVAL; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		Log::Warning("sys_send: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		return -ENOTSOCK;
	}
	
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)){
		Log::Warning("sys_send: Invalid buffer ptr");
		return -EFAULT;
	}
	
	socklen_t* slen = (socklen_t*)SC_ARG2(r);
	
	sockaddr_t* addr = (sockaddr_t*)SC_ARG1(r);

	Socket* sock = (Socket*)handle->node;
	return sock->ReceiveFrom(buffer, len, flags, addr, slen);
}

/* 
 * SetGetUID () - Get Process UID
 * 
 * On Success - Return process UID
 * On Failure - Does not fail
 */
long SysGetUID(RegisterContext* r){
	return Scheduler::GetCurrentProcess()->uid;
}

/* 
 * SetSetUID () - Set Process UID
 * 
 * On Success - Return process UID
 * On Failure - Return negative value
 */
long SysSetUID(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	uid_t requestedUID = SC_ARG0(r);

	if(proc->uid == requestedUID){
		return 0;
	}

	if(proc->euid == 0){
		proc->uid = requestedUID;
		proc->euid = requestedUID;
	} else {
		return -EPERM;
	}

	return 0;
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
long SysPoll(RegisterContext* r){
	pollfd* fds = (pollfd*)SC_ARG0(r);
	unsigned nfds = SC_ARG1(r);
	long timeout = SC_ARG2(r);

	Thread* thread = GetCPULocal()->currentThread;
	process_t* proc = Scheduler::GetCurrentProcess();
	if(!Memory::CheckUsermodePointer(SC_ARG0(r), nfds * sizeof(pollfd), proc->addressSpace)){
		Log::Warning("sys_poll: Invalid pointer to file descriptor array");
		return -EFAULT;
	}

	fs_fd_t* files[nfds];

	unsigned eventCount = 0; // Amount of fds with events
	for(unsigned i = 0; i < nfds; i++){
		fds[i].revents = 0;
		if(fds[i].fd < 0) continue;

		if((uint64_t)fds[i].fd >= Scheduler::GetCurrentProcess()->fileDescriptors.get_length()){
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
			if(!((Socket*)files[i]->node)->IsConnected() && !((Socket*)files[i]->node)->IsListening()){
				fds[i].revents |= POLLHUP;
				hasEvent = true;
			}
			
			if(((Socket*)files[i]->node)->PendingConnections() && (fds[i].events & POLLIN)){
				fds[i].revents |= POLLIN;
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

	if(!eventCount && timeout){
		timeval tVal = Timer::GetSystemUptimeStruct();

		FilesystemWatcher fsWatcher;
		for(unsigned i = 0; i < nfds; i++){
			fsWatcher.WatchNode(files[i]->node, fds[i].events);
		}

		if(timeout > 0){
			if(fsWatcher.WaitTimeout(timeout)){
				return -EINTR; // Interrupted
			} else if(timeout <= 0){
				return 0; // Timed out
			}
		} else if(fsWatcher.Wait()){
			return -EINTR; // Interrupted
		}

		do{
			if(eventCount > 0){
				break;
			}

			for(unsigned i = 0; i < nfds; i++){
				if(!files[i] || !files[i]->node) continue;

				bool hasEvent = 0;

				if((files[i]->node->flags & FS_NODE_TYPE) == FS_NODE_SOCKET){
					if(!((Socket*)files[i]->node)->IsConnected() && !((Socket*)files[i]->node)->IsListening()){
						fds[i].revents |= POLLHUP;
						hasEvent = 1;
					}

					if(((Socket*)files[i]->node)->PendingConnections() && (fds[i].events & POLLIN)){
						fds[i].revents |= POLLIN;
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

			if(eventCount){
				Scheduler::Yield();
			}
		} while(thread->state != ThreadStateZombie && (timeout < 0 || Timer::TimeDifference(Timer::GetSystemUptimeStruct(), tVal) < timeout)); // Wait until timeout, unless timeout is negative in which wait infinitely
	}
	
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
long SysSendMsg(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();

	if(SC_ARG0(r) >= proc->fileDescriptors.get_length()){
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("sys_sendmsg: Invalid File Descriptor: %d", SC_ARG0(r));
		});
		return -EBADF;
	}

	fs_fd_t* handle = proc->fileDescriptors.get_at(SC_ARG0(r));

	msghdr* msg = (msghdr*)SC_ARG1(r);
	uint64_t flags = SC_ARG3(r);

	if(!handle){ 
		
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("sys_sendmsg: Invalid file descriptor: ", SC_ARG0(r));
		});
		return -EBADF; 
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("sys_sendmsg: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		});
		return -ENOTSOCK;
	}
	
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(msghdr), proc->addressSpace)){
		
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("sys_sendmsg: Invalid msg ptr");
		});
		return -EFAULT;
	}
	
	if(!Memory::CheckUsermodePointer((uintptr_t)msg->msg_iov, sizeof(iovec) * msg->msg_iovlen, proc->addressSpace)){
		
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("sys_sendmsg: msg: Invalid iovec ptr");
		});
		return -EFAULT;
	}
	
	if(msg->msg_name && msg->msg_namelen && !Memory::CheckUsermodePointer((uintptr_t)msg->msg_name, msg->msg_namelen, proc->addressSpace)){
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("sys_sendmsg: msg: Invalid name ptr and name not null");
		});
		return -EFAULT;
	}

	if(msg->msg_control && msg->msg_controllen && !Memory::CheckUsermodePointer((uintptr_t)msg->msg_control, msg->msg_controllen, proc->addressSpace)){
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("sys_sendmsg: msg: Invalid control ptr and control null");
		});
		return -EFAULT;
	}

	long sent = 0;
	Socket* sock = (Socket*)handle->node;

	for(unsigned i = 0; i < msg->msg_iovlen; i++){
		if(!Memory::CheckUsermodePointer((uintptr_t)msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len, proc->addressSpace)){
			Log::Warning("sys_sendmsg: msg: Invalid iovec entry base");
			return -EFAULT;
		}

		long ret = sock->SendTo(msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len, flags, (sockaddr*)msg->msg_name, msg->msg_namelen, msg->msg_control, msg->msg_controllen);

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
long SysRecvMsg(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();

	if(SC_ARG0(r) >= proc->fileDescriptors.get_length()){
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("SysRecvMsg: Invalid File Descriptor: %d", SC_ARG0(r));
			});
		return -EBADF;
	}

	if(SC_ARG0(r) >= proc->fileDescriptors.get_length()){ 
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("SysRecvMsg: Invalid file descriptor: ", SC_ARG0(r));
		});
		return -EBADF; 
	}

	fs_fd_t* handle = proc->fileDescriptors.get_at(SC_ARG0(r));

	if(!handle){ 
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("SysRecvMsg: Invalid file descriptor: ", SC_ARG0(r));
		});
		return -EBADF; 
	}

	msghdr* msg = (msghdr*)SC_ARG1(r);
	uint64_t flags = SC_ARG3(r);

	if(handle->mode & O_NONBLOCK){
		flags |= MSG_DONTWAIT; // Don't wait if socket marked as nonblock
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("SysRecvMsg: File (Descriptor: %d) is not a socket", SC_ARG0(r));
		});
		return -ENOTSOCK;
	}
	
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(msghdr), proc->addressSpace)){
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("SysRecvMsg: Invalid msg ptr");
		});
		return -EFAULT;
	}
	
	if(!Memory::CheckUsermodePointer((uintptr_t)msg->msg_iov, sizeof(iovec) * msg->msg_iovlen, proc->addressSpace)){
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("SysRecvMsg: msg: Invalid iovec ptr");
		});
		return -EFAULT;
	}

	if(msg->msg_control && msg->msg_controllen && !Memory::CheckUsermodePointer((uintptr_t)msg->msg_control, msg->msg_controllen, proc->addressSpace)){
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("SysRecvMsg: msg: Invalid control ptr and control null");
		});
		return -EFAULT;
	}

	long read = 0;
	Socket* sock = (Socket*)handle->node;

	for(unsigned i = 0; i < msg->msg_iovlen; i++){
		if(!Memory::CheckUsermodePointer((uintptr_t)msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len, proc->addressSpace)){
			Log::Warning("SysRecvMsg: msg: Invalid iovec entry base");
			return -EFAULT;
		}

		socklen_t len = msg->msg_namelen;
		long ret = sock->ReceiveFrom(msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len, flags, reinterpret_cast<sockaddr*>(msg->msg_name), &len, msg->msg_control, msg->msg_controllen);
		msg->msg_namelen = len;

		if(ret < 0) {
			return ret;
		}

		read += ret;
	}

	return read;
}

/////////////////////////////
/// \brief SysGetEUID () - Set effective process UID
/// 
/// \return Process EUID (int)
/////////////////////////////
long SysGetEUID(RegisterContext* r){
	return Scheduler::GetCurrentProcess()->euid;
}

/////////////////////////////
/// \brief SysSetEUID () - Set effective process UID
/// 
/// \return On success return 0, otherwise return negative error code
/////////////////////////////
long SysSetEUID(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();
	uid_t requestedEUID = SC_ARG0(r);

	// Unprivileged processes may only set the effective user ID to the real user ID or the effective user ID
	if(proc->euid == requestedEUID){
		return 0;
	}

	if(proc->uid == 0 || proc->uid == requestedEUID){
		proc->euid = requestedEUID;
	} else {
		return -EPERM;
	}
	
	return 0;
}

/////////////////////////////
/// \brief SysGetProcessInfo (pid, pInfo)
///
/// \param pid - Process PID
/// \param pInfo - Pointer to lemon_process_info_t structure
///
/// \return On Success - Return 0
/// \return On Failure - Return error as negative value
/////////////////////////////
long SysGetProcessInfo(RegisterContext* r){
	uint64_t pid = SC_ARG0(r);
	lemon_process_info_t* pInfo = reinterpret_cast<lemon_process_info_t*>(SC_ARG1(r));

	process_t* cProcess = Scheduler::GetCurrentProcess();
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(lemon_process_info_t), cProcess->addressSpace)){
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

	pInfo->runningTime = Timer::GetSystemUptime() - reqProcess->creationTime.tv_sec;
	pInfo->activeUs = reqProcess->activeTicks * 1000000 / Timer::GetFrequency();

	pInfo->usedMem = reqProcess->usedMemoryBlocks / 4;
	return 0;
}

/////////////////////////////
/// \brief SysGetNextProcessInfo (pidP, pInfo)
///
/// \param pidP - Pointer to an unsigned integer holding a PID
/// \param pInfo - Pointer to process_info_t struct
///
/// \return On Success - Return 0
/// \return No more processes - Return 1
/// On Failure - Return error as negative value
/////////////////////////////
long SysGetNextProcessInfo(RegisterContext* r){
	pid_t* pidP = reinterpret_cast<pid_t*>(SC_ARG0(r));
	lemon_process_info_t* pInfo = reinterpret_cast<lemon_process_info_t*>(SC_ARG1(r));

	process_t* cProcess = Scheduler::GetCurrentProcess();
	if(!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(lemon_process_info_t), cProcess->addressSpace)){
		return -EFAULT;
	}

	if(!Memory::CheckUsermodePointer(SC_ARG0(r), sizeof(pid_t), cProcess->addressSpace)){
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

	pInfo->runningTime = Timer::GetSystemUptime() - reqProcess->creationTime.tv_sec;
	pInfo->activeUs = reqProcess->activeTicks * 1000000 / Timer::GetFrequency();

	pInfo->usedMem = reqProcess->usedMemoryBlocks / 4;
	return 0;
}

/////////////////////////////
/// \brief SysReadLink(pathname, buf, bufsize) Read a symbolic link
///
/// \param pathname - const char*, Path of the symbolic link
/// \param buf - char*, Buffer to store the link data
/// \param bufsize - size_t, Size of buf
///
/// \return Amount of bytes read on success
/// \return Negative value on failure
/////////////////////////////
long SysReadLink(RegisterContext* r){
	process_t* proc = Scheduler::GetCurrentProcess();

	if(!Memory::CheckUsermodePointer(SC_ARG0(r), 0, proc->addressSpace)){
		return -EFAULT; // Invalid path pointer
	}

	if(!Memory::CheckUsermodePointer(SC_ARG1(r), SC_ARG2(r), proc->addressSpace)){
		return -EFAULT; // Invalid buffer
	}

	const char* path = reinterpret_cast<const char*>(SC_ARG0(r));
	char* buffer = reinterpret_cast<char*>(SC_ARG1(r));
	size_t bufferSize = SC_ARG2(r);

	FsNode* link = fs::ResolvePath(path);
	if(!link){
		return -ENOENT; // path does not exist
	}

	return link->ReadLink(buffer, bufferSize);
}

/////////////////////////////
/// \brief SysSpawnThread(entry, stack) Spawn a new thread under current process
///
/// \param entry - (uintptr_t) Thread entry point
/// \param stack - (uintptr_t) Thread's stack
///
/// \return (pid_t) thread id
/////////////////////////////
long SysSpawnThread(RegisterContext* r){
	auto tid = Scheduler::CreateChildThread(Scheduler::GetCurrentProcess(), SC_ARG0(r), SC_ARG1(r), USER_CS, USER_SS);

	return tid;
}

/////////////////////////////
/// \brief SysExitThread(retval) Exit a thread
///
/// \param retval - (void*) Return value pointer
///
/// \return Undefined, always succeeds
/////////////////////////////
[[noreturn]]
long SysExitThread(RegisterContext* r){
	Log::Warning("SysExitThread is unimplemented! Hanging!");
	
	releaseLock(&GetCPULocal()->currentThread->lock);

	GetCPULocal()->currentThread->blocker = nullptr;
	GetCPULocal()->currentThread->state = ThreadStateBlocked;

	for(;;) {
		GetCPULocal()->currentThread->state = ThreadStateBlocked;
		Scheduler::Yield();
	}
}

/////////////////////////////
/// \brief SysFutexWake(futex) Wake a thread waiting on a futex
///
/// \param futex - (int*) Futex pointer
///
/// \return 0 on success, error code on failure
/////////////////////////////
long SysFutexWake(RegisterContext* r){
	int* futex = reinterpret_cast<int*>(SC_ARG0(r));

	if(!Memory::CheckUsermodePointer(SC_ARG0(r), sizeof(int), Scheduler::GetCurrentProcess()->addressSpace)){
		return EFAULT;
	}

	process_t* currentProcess = Scheduler::GetCurrentProcess();

	List<FutexThreadBlocker*>* blocked = nullptr;
	if(!currentProcess->futexWaitQueue.get(reinterpret_cast<uintptr_t>(futex), blocked) || !blocked->get_length()){
		return 0;
	}

	auto front = blocked->remove_at(0);

	if(front){
		front->Unblock();
	}

	return 0;
}

/////////////////////////////
/// \brief SysFutexWait(futex, expected) Wait on a futex.
///
/// Will wait on the futex if the value is equal to expected
///
/// \param futex (void*) Futex pointer
/// \param expected (int) Expected futex value
///
/// \return 0 on success, error code on failure
/////////////////////////////
long SysFutexWait(RegisterContext* r){
	int* futex = reinterpret_cast<int*>(SC_ARG0(r));

	if(!Memory::CheckUsermodePointer(SC_ARG0(r), sizeof(int), Scheduler::GetCurrentProcess()->addressSpace)){
		return EFAULT;
	}

	int expected = static_cast<int>(SC_ARG1(r));

	if(*futex != expected){
		return 0;
	}

	process_t* currentProcess = Scheduler::GetCurrentProcess();
	Thread* currentThread = Scheduler::GetCurrentThread();

	List<FutexThreadBlocker*>* blocked;
	if(!currentProcess->futexWaitQueue.get(reinterpret_cast<uintptr_t>(futex), blocked)){
		blocked = new List<FutexThreadBlocker*>();

		currentProcess->futexWaitQueue.insert(reinterpret_cast<uintptr_t>(futex), blocked);
	}

	FutexThreadBlocker blocker;
	blocked->add_back(&blocker);

	if(currentThread->Block(&blocker)){
		blocked->remove(&blocker);
		
		return -EINTR; // We were interrupted
	}

	return 0;
}

/////////////////////////////
/// \brief SysDup(fd) Duplicate a file descriptor
///
/// \param fd (int) file descriptor to duplicate
///
/// \return new file descriptor (int) on success, negative error code on failure
/////////////////////////////
long SysDup(RegisterContext* r){
	int fd = static_cast<int>(SC_ARG0(r));
	[[gnu::unused]] long flags = SC_ARG1(r);
	int requestedFd = static_cast<int>(SC_ARG2(r));

	fs_fd_t* handle;

	process_t* currentProcess = Scheduler::GetCurrentProcess();
	if(static_cast<unsigned>(fd) >= currentProcess->fileDescriptors.get_length() || !(handle = currentProcess->fileDescriptors[fd])){
		return -EBADF;
	}

	fs_fd_t* newHandle = new fs_fd_t;
	*newHandle = *handle;
	newHandle->node->handleCount++;

	int newFd = currentProcess->fileDescriptors.get_length();
	if(requestedFd >= 0){
		if(static_cast<unsigned>(requestedFd) >= currentProcess->fileDescriptors.get_length()){
			Log::Error("SysDup: We do not support (yet) requesting unallocated fds."); // TODO: Requested file descriptors out of range
			return -ENOSYS;
		}

		newFd = requestedFd;
		fs_fd_t* existingHandle = currentProcess->fileDescriptors[requestedFd];

		if(existingHandle){
			existingHandle->node->Close();
			delete existingHandle;
		}

		currentProcess->fileDescriptors[requestedFd] = newHandle;
	} else {
		currentProcess->fileDescriptors.add_back(newHandle);
	}

	return newFd;
}

/////////////////////////////
/// \brief SysGetFileStatusFlags(fd) Get a file handle's mode/status flags
///
/// \param fd (int) file descriptor
///
/// \return flags on success, negative error code on failure
/////////////////////////////
long SysGetFileStatusFlags(RegisterContext* r){
	int fd = static_cast<int>(SC_ARG0(r));
	fs_fd_t* handle;

	process_t* currentProcess = Scheduler::GetCurrentProcess();
	if(static_cast<unsigned>(fd) > currentProcess->fileDescriptors.get_length() || !(handle = currentProcess->fileDescriptors[fd])){
		return -EBADF;
	}

	return handle->mode;
}

/////////////////////////////
/// \brief SysSetFileStatusFlags(fd, flags) Set a file handle's mode/status flags
///
/// \param fd (int) file descriptor
/// \param flags (int) new status flags
///
/// \return 0 on success, negative error code on failure
/////////////////////////////
long SysSetFileStatusFlags(RegisterContext* r){
	int fd = static_cast<int>(SC_ARG0(r));
	int nFlags = static_cast<int>(SC_ARG1(r));
	fs_fd_t* handle;

	process_t* currentProcess = Scheduler::GetCurrentProcess();
	if(static_cast<unsigned>(fd) > currentProcess->fileDescriptors.get_length() || !(handle = currentProcess->fileDescriptors[fd])){
		return -EBADF;
	}

	int mask = (O_APPEND | O_NONBLOCK); // Only update append or nonblock
	
	handle->mode = (handle->mode & ~mask) | (nFlags & mask);

	return 0;
}

/////////////////////////////
/// \brief SysSelect(nfds, readfds, writefds, exceptfds, timeout) Set a file handle's mode/status flags
///
/// \param nfds (int) number of file descriptors
/// \param readfds (fd_set) check for readable fds
/// \param writefds (fd_set) check for writable fds
/// \param exceptfds (fd_set) check for exceptions on fds
/// \param timeout (timespec) timeout period
///
/// \return number of events on success, negative error code on failure
/////////////////////////////
long SysSelect(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	int nfds = static_cast<int>(SC_ARG0(r));
	fd_set_t* readFdsMask = reinterpret_cast<fd_set_t*>(SC_ARG1(r));
	fd_set_t* writeFdsMask = reinterpret_cast<fd_set_t*>(SC_ARG2(r));
	fd_set_t* exceptFdsMask = reinterpret_cast<fd_set_t*>(SC_ARG3(r));
	timespec_t* timeout = reinterpret_cast<timespec_t*>(SC_ARG4(r));

	if(!((!readFdsMask || Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(fd_set_t), currentProcess->addressSpace))
		&& (!writeFdsMask || Memory::CheckUsermodePointer(SC_ARG2(r), sizeof(fd_set_t), currentProcess->addressSpace))
		&& (!exceptFdsMask || Memory::CheckUsermodePointer(SC_ARG3(r), sizeof(fd_set_t), currentProcess->addressSpace))
		&& Memory::CheckUsermodePointer(SC_ARG4(r), sizeof(timespec_t), currentProcess->addressSpace))){
		return -EFAULT; // Only return EFAULT if read/write/exceptfds is not null
	}

	List<Pair<fs_fd_t*, int>> readfds;
	List<Pair<fs_fd_t*, int>> writefds;
	List<Pair<fs_fd_t*, int>> exceptfds;

	auto getHandleSafe = [&](int fd) -> fs_fd_t* {
		if(static_cast<unsigned>(fd) > currentProcess->fileDescriptors.get_length()){
			return nullptr;
		}
		return currentProcess->fileDescriptors[fd]; // If fd is null then this will return null as if out of range
	};

	for(int i = 0; i < 128 && i * 8 < nfds; i++){
		char read = 0, write = 0, except = 0;
		if(readFdsMask) {
			read = readFdsMask->fds_bits[i];
			readFdsMask->fds_bits[i] = 0; // With select the fds are cleared to 0 unless they are ready
		}

		if(writeFdsMask){
			write = writeFdsMask->fds_bits[i];
			writeFdsMask->fds_bits[i] = 0;
		}
		
		if(exceptFdsMask){
			except = exceptFdsMask->fds_bits[i];
			exceptFdsMask->fds_bits[i] = 0;
		}

		for(int j = 0; j < 8 && (i * 8 + j) < nfds; j++){
			if((read >> j) & 0x1){
				fs_fd_t* h = getHandleSafe(i * 8 + j);
				if(!h){
					return -EBADF;
				}

				readfds.add_back(Pair<fs_fd_t*, int>(h, i * 8 + j));
			}
			
			if((write >> j) & 0x1){
				fs_fd_t* h = getHandleSafe(i * 8 + j);
				if(!h){
					return -EBADF;
				}

				writefds.add_back(Pair<fs_fd_t*, int>(h, i * 8 + j));
			}
			
			if((except >> j) & 0x1){
				fs_fd_t* h = getHandleSafe(i * 8 + j);
				if(!h){
					return -EBADF;
				}

				exceptfds.add_back(Pair<fs_fd_t*, int>(h, i * 8 + j));
			}
		}
	}

	if(exceptfds.get_length()){
		//Log::Warning("SysSelect: ExceptFds ignored!");
	}

	int evCount = 0;
	
	for(auto& handle : readfds){
		if(handle.item1->node->CanRead()){
			FD_SET(handle.item2, readFdsMask);
			evCount++;
		}
	}

	for(auto& handle : writefds){
		if(handle.item1->node->CanWrite()){
			FD_SET(handle.item2, writeFdsMask);
			evCount++;
		}
	}

	if(evCount){
		return evCount;
	}

	timeval timeEnd = {.tv_sec = timeout->tv_sec + Timer::GetSystemUptimeStruct().tv_sec, .tv_usec = timeout->tv_nsec / 1000 + Timer::GetSystemUptimeStruct().tv_usec};
	while(Timer::GetSystemUptimeStruct() < timeEnd){
		evCount = 0;

		for(auto& handle : readfds){
			if(handle.item1->node->CanRead()){
				FD_SET(handle.item2, readFdsMask);
				evCount++;
			}
		}

		for(auto& handle : writefds){
			if(handle.item1->node->CanWrite()){
				FD_SET(handle.item2, writeFdsMask);
				evCount++;
			}
		}

		//for(fs_fd_t* handle : exceptfds);

		if(evCount){
			break;
		}

		Scheduler::Yield();
	}

	return evCount;
}

/////////////////////////////
/// \brief SysCreateService (name) - Create a service
///
/// Create a new service. Services are essentially named containers for interfaces, allowing one program to implement multiple interfaces under a service.
///
/// \param name (const char*) Service name to be used, must be unique
///
/// \return Handle ID on success, error code on failure
/////////////////////////////
long SysCreateService(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	size_t nameLength;
	if(strlenSafe(reinterpret_cast<const char*>(SC_ARG0(r)), nameLength, currentProcess->addressSpace)){
		return -EFAULT;
	}

	char name[nameLength + 1];
	strncpy(name, reinterpret_cast<const char*>(SC_ARG0(r)), nameLength);
	name[nameLength] = 0;
	for(auto& svc : ServiceFS::Instance()->services){
		if(strncmp(svc->GetName(), name, nameLength) == 0){
			return -EEXIST;
		}
	}

	FancyRefPtr<Service> svc = ServiceFS::Instance()->CreateService(name);
	Handle& handle = Scheduler::RegisterHandle(currentProcess, static_pointer_cast<KernelObject, Service>(svc));

	return handle.id;
}

/////////////////////////////
/// \brief SysCreateInterface (service, name, msgSize) - Create a new interface
///
/// Create a new interface. Interfaces allow clients to open connections to a service
///
/// \param service (handle_id_t) Handle ID of the service hosting the interface
/// \param name (const char*) Name of the interface, 
/// \param msgSize (uint16_t) Maximum message size for all connections
///
/// \return Handle ID of service on success, negative error code on failure
/////////////////////////////
long SysCreateInterface(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	Handle* svcHandle;
	if(Scheduler::FindHandle(currentProcess, SC_ARG0(r), &svcHandle)){
		Log::Warning("SysCreateInterface: Invalid handle ID %d", SC_ARG0(r));
		return -EINVAL;
	}

	if(!svcHandle->ko->IsType(Service::TypeID())){
		Log::Warning("SysCreateInterface: Invalid handle type (ID %d)", SC_ARG0(r));
		return -EINVAL;
	}

	size_t nameLength;
	if(strlenSafe(reinterpret_cast<const char*>(SC_ARG1(r)), nameLength, currentProcess->addressSpace)){
		return -EFAULT;
	}

	char name[nameLength + 1];
	strncpy(name, reinterpret_cast<const char*>(SC_ARG1(r)), nameLength);
	name[nameLength] = 0;

	Service* svc = reinterpret_cast<Service*>(svcHandle->ko.get());

	FancyRefPtr<MessageInterface> interface;
	long ret = svc->CreateInterface(interface, name, SC_ARG2(r));

	if(ret){
		return ret;
	}

	Handle& handle = Scheduler::RegisterHandle(currentProcess, static_pointer_cast<KernelObject, MessageInterface>(interface));

	return handle.id;
}

/////////////////////////////
/// \brief SysInterfaceAccept (interface) - Accept connections on an interface
///
/// Accept a pending connection on an interface and return a new MessageEndpoint
///
/// \param interface (handle_id_t) Handle ID of the interface
///
/// \return Handle ID of endpoint on success, 0 when no pending connections, negative error code on failure
/////////////////////////////
long SysInterfaceAccept(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	Handle* ifHandle;
	if(Scheduler::FindHandle(currentProcess, SC_ARG0(r), &ifHandle)){
		Log::Warning("SysInterfaceAccept: Invalid handle ID %d", SC_ARG0(r));
		return -EINVAL;
	}

	if(!ifHandle->ko->IsType(MessageInterface::TypeID())){
		Log::Warning("SysInterfaceAccept: Invalid handle type (ID %d)", SC_ARG0(r));
		return -EINVAL;
	}

	MessageInterface* interface = reinterpret_cast<MessageInterface*>(ifHandle->ko.get());
	FancyRefPtr<MessageEndpoint> endp;
	if(long ret = interface->Accept(endp); ret <= 0){
		return ret;
	}

	Handle& handle = Scheduler::RegisterHandle(currentProcess, static_pointer_cast<KernelObject, MessageEndpoint>(endp));

	return handle.id;
}

/////////////////////////////
/// \brief SysInterfaceConnect (path) - Open a connection to an interface
///
/// Open a new connection on an interface and return a new MessageEndpoint
///
/// \param interface (const char*) Path of interface in format servicename/interfacename (e.g. lemon.lemonwm/wm)
///
/// \return Handle ID of endpoint on success, negative error code on failure
/////////////////////////////
long SysInterfaceConnect(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	size_t sz = 0;
	if(strlenSafe(reinterpret_cast<const char*>(SC_ARG0(r)), sz, currentProcess->addressSpace)){
		return -EFAULT;
	}

	char path[sz + 1];
	strncpy(path, reinterpret_cast<const char*>(SC_ARG0(r)), sz);
	path[sz] = 0;

	if(!strchr(path, '/')){ // Interface name given by '/' separator
		Log::Warning("SysInterfaceConnect: No interface name given!");
		return -EINVAL;
	}

	FancyRefPtr<MessageInterface> interface;
	{
		FancyRefPtr<Service> svc;
		if(ServiceFS::Instance()->ResolveServiceName(svc, path)){
			return -ENOENT; // No such service
		}
	
		if(svc->ResolveInterface(interface, strchr(path, '/') + 1)){
			return -ENOENT; // No such interface
		}
	}

	FancyRefPtr<MessageEndpoint> endp = interface->Connect();
	if(!endp.get()){
		return -EINVAL; // Some error connecting, interface destroyed? 
	}
	
	Handle& handle = Scheduler::RegisterHandle(currentProcess, static_pointer_cast<KernelObject>(endp));

	return handle.id;
}

/////////////////////////////
/// \brief SysEndpointQueue (endpoint, id, size, data) - Queue a message on an endpoint
///
/// Queues a new message on the specified endpoint.
///
/// \param endpoint (handle_id_t) Handle ID of specified endpoint
/// \param id (uint64_t) Message ID
/// \param size (uint64_t) Message Size
/// \param data (uint8_t*) Pointer to message data
///
/// \return 0 on success, negative error code on failure
/////////////////////////////
long SysEndpointQueue(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	Handle* endpHandle;
	if(Scheduler::FindHandle(currentProcess, SC_ARG0(r), &endpHandle)){
		IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
			Log::Warning("(%s): SysEndpointQueue: Invalid handle ID %d", currentProcess->name, SC_ARG0(r));
			Log::Info("%x", r->rip);
			UserPrintStackTrace(r->rbp, Scheduler::GetCurrentProcess()->addressSpace);
		});
		return -EINVAL;
	}

	if(!endpHandle->ko->IsType(MessageEndpoint::TypeID())){
		Log::Warning("(%s): SysEndpointQueue: Invalid handle type (ID %d)", SC_ARG0(r));
		return -EINVAL;
	}

	size_t size = SC_ARG2(r);
	if(!Memory::CheckUsermodePointer(SC_ARG3(r), size, currentProcess->addressSpace)){
		return -EFAULT; // Data greater than 8 and invalid pointer
	}

	MessageEndpoint* endpoint = reinterpret_cast<MessageEndpoint*>(endpHandle->ko.get());

	return endpoint->Write(SC_ARG1(r), size, SC_ARG3(r));
}

/////////////////////////////
/// \brief SysEndpointDequeue (endpoint, id, size, data)
///
/// Accept a pending connection on an interface and return a new MessageEndpoint
///
/// \param endpoint (handle_id_t) Handle ID of specified endpoint
/// \param id (uint64_t*) Returned message ID
/// \param size (uint32_t*) Returned message Size
/// \param data (uint8_t*) Message data buffer
///
/// \return 0 on empty, 1 on success, negative error code on failure
/////////////////////////////
long SysEndpointDequeue(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	Handle* endpHandle;
	if(Scheduler::FindHandle(currentProcess, SC_ARG0(r), &endpHandle)){
		Log::Warning("(%s): SysEndpointDequeue: Invalid handle ID %d", currentProcess->name, SC_ARG0(r));
		return -EINVAL;
	}

	if(!endpHandle->ko->IsType(MessageEndpoint::TypeID())){
		Log::Warning("SysEndpointDequeue: Invalid handle type (ID %d)", SC_ARG0(r));
		return -EINVAL;
	}

	if(!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(uint64_t), currentProcess->addressSpace)){
		return -EFAULT;
	}

	if(!Memory::CheckUsermodePointer(SC_ARG2(r), sizeof(uint16_t), currentProcess->addressSpace)){
		return -EFAULT;
	}

	MessageEndpoint* endpoint = reinterpret_cast<MessageEndpoint*>(endpHandle->ko.get());

	if(!Memory::CheckUsermodePointer(SC_ARG3(r), endpoint->GetMaxMessageSize(), currentProcess->addressSpace)){
		return -EFAULT;
	}

	return endpoint->Read(reinterpret_cast<uint64_t*>(SC_ARG1(r)), reinterpret_cast<uint16_t*>(SC_ARG2(r)), reinterpret_cast<uint8_t*>(SC_ARG3(r)));
}

/////////////////////////////
/// \brief SysEndpointCall (endpoint, id, data, rID, rData, size, timeout)
///
/// Accept a pending connection on an interface and return a new MessageEndpoint
///
/// \param endpoint (handle_id_t) Handle ID of specified endpoint
/// \param id (uint64_t) id of message to send
/// \param data (uint8_t*) Message data to be sent
/// \param rID (uint64_t) id of expected returned message
/// \param rData (uint8_t*) Return message data buffer
/// \param size (uint32_t*) Pointer containing size of message to be sent and size of returned message
/// \param timeout (int64_t) timeout in us
///
/// \return 0 on success, negative error code on failure
/////////////////////////////
long SysEndpointCall(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	Handle* endpHandle;
	if(Scheduler::FindHandle(currentProcess, SC_ARG0(r), &endpHandle)){
		Log::Warning("SysEndpointCall: Invalid handle ID %d", SC_ARG0(r));
		return -EINVAL;
	}

	if(!endpHandle->ko->IsType(MessageEndpoint::TypeID())){
		Log::Warning("SysEndpointCall: Invalid handle type (ID %d)", SC_ARG0(r));
		return -EINVAL;
	}

	uint16_t* size = reinterpret_cast<uint16_t*>(SC_ARG5(r));
	if(!Memory::CheckUsermodePointer(SC_ARG3(r), *size, currentProcess->addressSpace)){
		return -EFAULT; // Size greater than 8 and invalid data pointer
	}

	MessageEndpoint* endpoint = reinterpret_cast<MessageEndpoint*>(endpHandle->ko.get());

	return endpoint->Call(SC_ARG1(r), *size, SC_ARG2(r), SC_ARG3(r), size, reinterpret_cast<uint8_t*>(SC_ARG4(r)), -1);
}

/////////////////////////////
/// \brief SysEndpointInfo (endpoint, info)
///
/// Get endpoint information
///
/// \param endpoint Endpoint handle
/// \param info Pointer to endpoint info struct
///
/// \return 0 on success, negative error code on failure
/////////////////////////////
long SysEndpointInfo(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	MessageEndpointInfo* info = reinterpret_cast<MessageEndpointInfo*>(SC_ARG1(r));

	if(!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(MessageEndpointInfo), currentProcess->addressSpace)){
		return -EFAULT; // Data greater than 8 and invalid pointer
	}

	Handle* endpHandle;
	if(Scheduler::FindHandle(currentProcess, SC_ARG0(r), &endpHandle)){
		Log::Warning("SysEndpointInfo: Invalid handle ID %d", SC_ARG0(r));
		return -EINVAL;
	}

	if(!endpHandle->ko->IsType(MessageEndpoint::TypeID())){
		Log::Warning("SysEndpointInfo: Invalid handle type (ID %d)", SC_ARG0(r));
		return -EINVAL;
	}

	MessageEndpoint* endpoint = reinterpret_cast<MessageEndpoint*>(endpHandle->ko.get());

	*info = { .msgSize = endpoint->GetMaxMessageSize() };
	return 0;
}

/////////////////////////////
/// \brief SysKernelObjectWaitOne (object)
///
/// Wait on one KernelObject
///
/// \param object (handle_t) Object to wait on
/// \param timeout (long) Timeout in microseconds
///
/// \return negative error code on failure
/////////////////////////////
long SysKernelObjectWaitOne(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	Handle* handle;
	if(Scheduler::FindHandle(currentProcess, SC_ARG0(r), &handle)){
		Log::Warning("SysKernelObjectWaitOne: Invalid handle ID %d", SC_ARG0(r));
		return -EINVAL;
	}

	long timeout = SC_ARG1(r);

	KernelObjectWatcher watcher;

	watcher.WatchObject(handle->ko, 0);
	
	if(timeout > 0){
		if(watcher.WaitTimeout(timeout)){
			return -EINTR;
		}
	} else {
		if(watcher.Wait()){
			return -EINTR;
		}
	}

	return 0;
}

/////////////////////////////
/// \brief SysKernelObjectWait (objects, count)
///
/// Wait on one KernelObject
///
/// \param objects (handle_t*) Pointer to array of handles to wait on
/// \param count (size_t) Amount of objects to wait on
/// \param timeout (long) Timeout in microseconds
///
/// \return negative error code on failure
/////////////////////////////
long SysKernelObjectWait(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();
	unsigned count = SC_ARG1(r);
	long timeout = SC_ARG2(r);

	if(!Memory::CheckUsermodePointer(SC_ARG0(r), count * sizeof(handle_id_t), currentProcess->addressSpace)){
		return -EFAULT;
	}

	Handle* handles[count];

	KernelObjectWatcher watcher;
	for(unsigned i = 0; i < count; i++){
		if(Scheduler::FindHandle(currentProcess, reinterpret_cast<handle_id_t*>(SC_ARG0(r))[i], &handles[i])){
			IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
				Log::Warning("SysKernelObjectWait: Invalid handle ID %d", SC_ARG0(r));
			});
			return -EINVAL;
		}

		watcher.WatchObject(handles[i]->ko, 0);
	}

	if(timeout > 0){
		if(watcher.WaitTimeout(timeout)){
			return -EINTR;
		}
	} else {
		if(watcher.Wait()){
			return -EINTR;
		}
	}

	return 0;
}

/////////////////////////////
/// \brief SysKernelObjectDestroy (object)
///
/// Destroy a KernelObject
///
/// \param object (handle_t) Object to destroy
///
/// \return negative error code on failure
/////////////////////////////
long SysKernelObjectDestroy(RegisterContext* r){
	process_t* currentProcess = Scheduler::GetCurrentProcess();

	Handle* h;
	if(Scheduler::FindHandle(currentProcess, SC_ARG0(r), &h)){
		Log::Warning("SysKernelObjectDestroy: Invalid handle ID %d", SC_ARG0(r));
		
		IF_DEBUG(debugLevelSyscalls >= DebugLevelVerbose, {
			Log::Info("Process %s (PID: %d), Stack Trace:", currentProcess->name, currentProcess->pid);
			Log::Info("%x", r->rip);
			UserPrintStackTrace(r->rbp, currentProcess->addressSpace);
		});
		return -EINVAL;
	}

	h->ko->Destroy();

	Scheduler::DestroyHandle(currentProcess, SC_ARG0(r));
	return 0;
}

/////////////////////////////
/// \brief SysSetSocketOptions(sockfd, level, optname, optval, optlen)
///
/// Set options on sockets 
///
/// \param sockfd Socket file desscriptor
/// \param level
/// \param optname
/// \param optval
/// \param optlen
///
/// \return 0 on success negative error code on failure
/////////////////////////////
long SysSetSocketOptions(RegisterContext* r){
	int fd = SC_ARG0(r);
	int level = SC_ARG1(r);
	int opt = SC_ARG2(r);
	const void* optVal = reinterpret_cast<void*>(SC_ARG3(r));
	socklen_t optLen = SC_ARG4(r);

	fs_fd_t* handle;
	process_t* currentProcess = Scheduler::GetCurrentProcess();
	if(static_cast<unsigned>(fd) >= currentProcess->fileDescriptors.get_length() || !(handle = currentProcess->fileDescriptors[fd])){
		return -EBADF;
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		return -ENOTSOCK; //  Not a socket
	}

	if(opt && optLen && !Memory::CheckUsermodePointer(SC_ARG3(r), optLen, currentProcess->addressSpace)){
		return -EFAULT;
	}

	Socket* sock = reinterpret_cast<Socket*>(handle->node);
	return sock->SetSocketOptions(level, opt, optVal, optLen);
}

/////////////////////////////
/// \brief SysGetSocketOptions(sockfd, level, optname, optval, optlen)
///
/// Get options on sockets 
///
/// \param sockfd Socket file desscriptor
/// \param level
/// \param optname
/// \param optval
/// \param optlen
///
/// \return 0 on success negative error code on failure
/////////////////////////////
long SysGetSocketOptions(RegisterContext* r){
	int fd = SC_ARG0(r);
	int level = SC_ARG1(r);
	int opt = SC_ARG2(r);
	void* optVal = reinterpret_cast<void*>(SC_ARG3(r));
	socklen_t* optLen = reinterpret_cast<socklen_t*>(SC_ARG4(r));

	fs_fd_t* handle;
	process_t* currentProcess = Scheduler::GetCurrentProcess();
	if(static_cast<unsigned>(fd) >= currentProcess->fileDescriptors.get_length() || !(handle = currentProcess->fileDescriptors[fd])){
		return -EBADF;
	}

	if((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET){
		return -ENOTSOCK; //  Not a socket
	}

	if(opt && *optLen && !Memory::CheckUsermodePointer(SC_ARG3(r), *optLen, currentProcess->addressSpace)){
		return -EFAULT;
	}

	Socket* sock = reinterpret_cast<Socket*>(handle->node);
	return sock->GetSocketOptions(level, opt, optVal, optLen);
}

long SysDeviceManagement(RegisterContext* r){
	process_t* process = Scheduler::GetCurrentProcess();

	int64_t request = SC_ARG0(r);

	if(request == DeviceManager::RequestDeviceManagerGetRootDeviceCount){
		return DeviceManager::DeviceCount();
	} else if(request == DeviceManager::RequestDeviceManagerEnumerateRootDevices){
		int64_t offset = SC_ARG1(r);
		int64_t requestedDeviceCount = SC_ARG2(r);
		int64_t* idList = reinterpret_cast<int64_t*>(SC_ARG3(r));

		if(!Memory::CheckUsermodePointer(SC_ARG3(r), sizeof(int64_t) * requestedDeviceCount, process->addressSpace)){
			return -EFAULT;
		}

		return DeviceManager::EnumerateDevices(offset, requestedDeviceCount, idList);
	}
	
	switch(request){
	case DeviceManager::RequestDeviceResolveFromPath: {
		const char* path = reinterpret_cast<const char*>(SC_ARG1(r));

		size_t pathLen;
		if(strlenSafe(path, pathLen, process->addressSpace)){
			return -EFAULT;
		}

		Device* dev = DeviceManager::ResolveDevice(path);
		if(!dev){
			return -ENOENT; // No such device 
		}

		return dev->ID();
	} case DeviceManager::RequestDeviceGetName: {
		int64_t deviceID = SC_ARG1(r);
		char* name = reinterpret_cast<char*>(SC_ARG2(r));
		size_t nameBufferSize = SC_ARG3(r);

		if(!Memory::CheckUsermodePointer(SC_ARG2(r), nameBufferSize, process->addressSpace)){
			return -EFAULT;
		}

		Device* dev = DeviceManager::DeviceFromID(deviceID);
		if(!dev){
			return -ENOENT;
		}

		strncpy(name, dev->DeviceName(), nameBufferSize);
		return 0;
	} case DeviceManager::RequestDeviceGetInstanceName: {
		int64_t deviceID = SC_ARG1(r);
		char* name = reinterpret_cast<char*>(SC_ARG2(r));
		size_t nameBufferSize = SC_ARG3(r);

		if(!Memory::CheckUsermodePointer(SC_ARG2(r), nameBufferSize, process->addressSpace)){
			return -EFAULT;
		}

		Device* dev = DeviceManager::DeviceFromID(deviceID);
		if(!dev){
			return -ENOENT;
		}

		strncpy(name, dev->InstanceName(), nameBufferSize);
		return 0;
	} case DeviceManager::RequestDeviceGetPCIInformation:
		return -ENOSYS;
	case DeviceManager::RequestDeviceIOControl:
		return -ENOSYS;
	case DeviceManager::RequestDeviceGetType: {
		long deviceID = SC_ARG1(r);

		if(!Memory::CheckUsermodePointer(SC_ARG2(r), sizeof(long), process->addressSpace)){
			return -EFAULT;
		}

		Device* dev = DeviceManager::DeviceFromID(deviceID);
		if(!dev){
			return -ENOENT;
		}

		return dev->Type();
	} case DeviceManager::RequestDeviceGetChildCount:
		return -ENOSYS;
	case DeviceManager::RequestDeviceEnumerateChildren:
		return -ENOSYS;
	default:
		return -EINVAL;
	}
}

long SysInterruptThread(RegisterContext* r){
	process_t* process = Scheduler::GetCurrentProcess();

	long tid = SC_ARG0(r);

	if(tid < 0 || tid > process->threads.get_length()){
		return -EINVAL; // Thread does not exist
	}

	Thread* th = process->threads.get_at(tid);
	if(!th){
		return -ESRCH; // Thread has already been killed
	}
	
	if(th->blocker){
		th->blocker->Interrupt(); // Stop the thread from blocking
		th->Unblock();
	}

	return 0;
}

long SysLoadKernelModule(RegisterContext* r){
	process_t* process = Scheduler::GetCurrentProcess();

	if(process->euid != 0){
		return -EPERM; // Must be root
	}

	size_t filePathLength;
	long filePathInvalid = strlenSafe(reinterpret_cast<char*>(SC_ARG0(r)), filePathLength, process->addressSpace);
	if(filePathInvalid){
		Log::Warning("SysExec: Reached unallocated memory reading file path");
		return -EFAULT;
	}

	char filepath[filePathLength + 1];
	strncpy(filepath, (char*)SC_ARG0(r), filePathLength);

	ModuleLoadStatus status = ModuleManager::LoadModule(filepath);
	if(status.status != ModuleLoadStatus::ModuleOK){
		return -EINVAL;
	} else {
		return 0;
	}
}

long SysUnloadKernelModule(RegisterContext* r){
	process_t* process = Scheduler::GetCurrentProcess();

	if(process->euid != 0){
		return -EPERM; // Must be root
	}

	size_t nameLength;
	long nameInvalid = strlenSafe(reinterpret_cast<char*>(SC_ARG0(r)), nameLength, process->addressSpace);
	if(nameInvalid){
		Log::Warning("SysExec: Reached unallocated memory reading file path");
		return -EFAULT;
	}

	char name[nameLength + 1];
	strncpy(name, (char*)SC_ARG0(r), nameLength);

	return ModuleManager::UnloadModule(name);
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
	SysReadLink,
	SysSpawnThread,
	SysExitThread,
	SysFutexWake,				// 70
	SysFutexWait,
	SysDup,
	SysGetFileStatusFlags,
	SysSetFileStatusFlags,
	SysSelect,					// 75
	SysCreateService,
	SysCreateInterface,
	SysInterfaceAccept,
	SysInterfaceConnect,
	SysEndpointQueue,			// 80
	SysEndpointDequeue,
	SysEndpointCall,
	SysEndpointInfo,
	SysKernelObjectWaitOne,
	SysKernelObjectWait,		// 85
	SysKernelObjectDestroy,
	SysSetSocketOptions,
	SysGetSocketOptions,
	SysDeviceManagement,
	SysInterruptThread,
	SysLoadKernelModule,
	SysUnloadKernelModule,
};

RegisterContext lastSyscall;

void DumpLastSyscall(){
	Log::Info("Last syscall:\nCall: %d, arg0: %i (%x), arg1: %i (%x), arg2: %i (%x), arg3: %i (%x), arg4: %i (%x), arg5: %i (%x)",
		lastSyscall.rax,
		SC_ARG0(&lastSyscall), SC_ARG0(&lastSyscall),
		SC_ARG1(&lastSyscall), SC_ARG1(&lastSyscall),
		SC_ARG2(&lastSyscall), SC_ARG2(&lastSyscall),
		SC_ARG3(&lastSyscall), SC_ARG3(&lastSyscall),
		SC_ARG4(&lastSyscall), SC_ARG4(&lastSyscall),
		SC_ARG5(&lastSyscall), SC_ARG5(&lastSyscall));
}

extern "C"
void SyscallHandler(RegisterContext* regs) {
	if (regs->rax >= NUM_SYSCALLS || !syscalls[regs->rax]) // If syscall is non-existant then return
		return;
		
	asm("sti"); // By reenabling interrupts, a thread in a syscall can be preempted

	Thread* thread = GetCPULocal()->currentThread;
	if(thread->state == ThreadStateZombie) for(;;);

	#ifdef KERNEL_DEBUG
	uint64_t rsp = 0;
	asm volatile("mov %%rsp, %0" : "=r"(rsp));

	if(rsp < KERNEL_VIRTUAL_BASE){
		Log::Info("warning: call: %d, thread rip: %x, kstack: %x, kernel rsp: %x, tss rsp0: %x, thread cs: %x, thread ss: %x", regs->rax, regs->rip, GetCPULocal()->currentThread->kernelStack, rsp, GetCPULocal()->tss.rsp0, regs->cs, regs->ss);
	}

	if(debugLevelSyscalls >= DebugLevelNormal){
		lastSyscall = *regs;
	}
	#endif

	if(acquireTestLock(&thread->lock)){
		for(;;);
	}
	
	regs->rax = syscalls[regs->rax](regs); // Call syscall
	releaseLock(&thread->lock);
}