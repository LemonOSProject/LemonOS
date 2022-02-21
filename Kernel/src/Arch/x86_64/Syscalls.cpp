#include <Syscalls.h>

#include <CPU.h>
#include <Debug.h>
#include <Device.h>
#include <Errno.h>
#include <Framebuffer.h>
#include <Fs/EPoll.h>
#include <Fs/Pipe.h>
#include <HAL.h>
#include <IDT.h>
#include <Lemon.h>
#include <Lock.h>
#include <Logging.h>
#include <Math.h>
#include <Modules.h>
#include <Net/Socket.h>
#include <Objects/Service.h>
#include <OnCleanup.h>
#include <Pair.h>
#include <PhysicalAllocator.h>
#include <SMP.h>
#include <Scheduler.h>
#include <SharedMemory.h>
#include <Signal.h>
#include <StackTrace.h>
#include <TTY/PTY.h>
#include <Timer.h>
#include <UserPointer.h>
#include <Video/Video.h>

#include <ABI/Process.h>

#include <abi-bits/vm-flags.h>
#include <sys/ioctl.h>

#define EXEC_CHILD 1

#define WCONTINUED 1
#define WNOHANG 2
#define WUNTRACED 4
#define WEXITED 8
#define WNOWAIT 16
#define WSTOPPED 32

#define SC_TRY_OR_ERROR(func) ({auto result = func; if (!result) { return -result.err.code; } std::move(result.Value());})

long SysExit(RegisterContext* r) {
    int code = SC_ARG0(r);

    Log::Info("Process %s (PID: %d) exiting with code %d", Scheduler::GetCurrentProcess()->name,
              Scheduler::GetCurrentProcess()->PID(), code);

    IF_DEBUG(debugLevelSyscalls >= DebugLevelVerbose, {
        Log::Info("rip: %x", r->rip);
        UserPrintStackTrace(r->rbp, Scheduler::GetCurrentProcess()->addressSpace);
    });

    Process::Current()->exitCode = code;
    Process::Current()->Die();
    return 0;
}

long SysExec(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    size_t filePathLength;
    long filePathInvalid =
        strlenSafe(reinterpret_cast<char*>(SC_ARG0(r)), filePathLength, currentProcess->addressSpace);
    if (filePathInvalid) {
        Log::Warning("SysExec: Reached unallocated memory reading file path");
        return -EFAULT;
    }

    char filepath[filePathLength + 1];

    strncpy(filepath, (char*)SC_ARG0(r), filePathLength);
    int argc = SC_ARG1(r);
    char** argv = (char**)SC_ARG2(r);
    uint64_t flags = SC_ARG3(r);
    char** envp = (char**)SC_ARG4(r);

    FsNode* node = fs::ResolvePath(filepath, currentProcess->workingDir->node, true /* Follow Symlinks */);
    if (!node) {
        return -ENOENT;
    }

    Vector<String> kernelEnvp;
    if (envp) {
        int i = 0;
        while (envp[i]) {
            size_t len;
            if (strlenSafe(envp[i], len, currentProcess->addressSpace)) {
                Log::Warning("SysExec: Reached unallocated memory reading environment");
                return -EFAULT;
            }

            kernelEnvp.add_back(envp[i]);
            i++;
        }
    }

    Log::Info("Loading: %s", (char*)SC_ARG0(r));

    Vector<String> kernelArgv;
    for (int i = 0; i < argc; i++) {
        if (!argv[i]) { // Some programs may attempt to terminate argv with a null pointer
            argc = i;
            break;
        }

        size_t len;
        if (strlenSafe(argv[i], len, currentProcess->addressSpace)) {
            Log::Warning("SysExec: Reached unallocated memory reading argv");
            return -EFAULT;
        }

        kernelArgv.add_back(String(argv[i]));
    }

    if (!kernelArgv.size()) {
        kernelArgv.add_back(filepath); // Ensure at least argv[0] is set
    }

    timeval tv = Timer::GetSystemUptimeStruct();
    uint8_t* buffer = (uint8_t*)kmalloc(node->size);
    size_t read = fs::Read(node, 0, node->size, buffer);
    if (read != node->size) {
        Log::Warning("Could not read file: %s", filepath);
        return 0;
    }
    timeval tvnew = Timer::GetSystemUptimeStruct();
    Log::Info("Done (took %d us)", Timer::TimeDifference(tvnew, tv));
    FancyRefPtr<Process> proc = Process::CreateELFProcess((void*)buffer, kernelArgv, kernelEnvp, filepath,
                                                          ((flags & EXEC_CHILD) ? currentProcess : nullptr));
    kfree(buffer);

    if (!proc) {
        Log::Warning("SysExec: Proc is null!");
        return -EIO; // Failed to create process
    }

    proc->workingDir = currentProcess->workingDir;
    strncpy(proc->workingDirPath, currentProcess->workingDirPath, PATH_MAX);

    if (flags & EXEC_CHILD) {
        currentProcess->RegisterChildProcess(proc);

        // Copy handles
        proc->m_handles.resize(currentProcess->HandleCount());
        for(unsigned i = 0; i < proc->m_handles.size(); i++) {
            if(!currentProcess->m_handles[i].closeOnExec) {
                proc->m_handles[i] = currentProcess->m_handles[i];
            } else {
                proc->m_handles[i] = HANDLE_NULL;
            }
        }
    }

    proc->Start();
    return proc->PID();
}

long SysRead(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysRead: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    uint8_t* buffer = (uint8_t*)SC_ARG1(r);
    uint64_t count = SC_ARG2(r);

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), count, proc->addressSpace)) {
        Log::Warning("SysRead: Invalid Memory Buffer: %x", SC_ARG1(r));
        return -EFAULT;
    }

    ssize_t ret = fs::Read(handle, count, buffer);
    return ret;
}

long SysWrite(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysWrite: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    uint8_t* buffer = (uint8_t*)SC_ARG1(r);
    uint64_t count = SC_ARG2(r);

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), count, proc->addressSpace)) {
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
long SysOpen(RegisterContext* r) {
    char* filepath = (char*)kmalloc(strlen((char*)SC_ARG0(r)) + 1);
    strcpy(filepath, (char*)SC_ARG0(r));
    FsNode* root = fs::GetRoot();

    uint64_t flags = SC_ARG1(r);

    Process* proc = Scheduler::GetCurrentProcess();

    IF_DEBUG(debugLevelSyscalls >= DebugLevelVerbose, { Log::Info("Opening: %s (flags: %u)", filepath, flags); });

    if (strcmp(filepath, "/") == 0) {
        return proc->AllocateHandle(fs::Open(root, 0).Value());
    }

open:
    Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "Working dir: %s", proc->workingDirPath);
    FsNode* node = fs::ResolvePath(filepath, proc->workingDir->node, !(flags & O_NOFOLLOW));

    if (!node) {
        if (flags & O_CREAT) {
            FsNode* parent = fs::ResolveParent(filepath, proc->workingDir->node);
            char* basename = fs::BaseName(filepath);

            if (!parent) {
                Log::Warning("SysOpen: Could not resolve parent directory of new file %s", basename);
                return -ENOENT;
            }

            DirectoryEntry ent;
            strcpy(ent.name, basename);

            IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, { Log::Info("SysOpen: Creating %s", filepath); });

            parent->Create(&ent, flags);

            kfree(basename);

            flags &= ~O_CREAT;
            goto open;
        } else {
            IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal,
                     { Log::Warning("SysOpen (flags %x): Failed to open file %s", flags, filepath); });
            return -ENOENT;
        }
    } else if (node->IsSymlink()) {
        Log::Warning("SysOpen: Is a symlink (O_NOFOLLOW specified)");
        return -ELOOP;
    }

    if (flags & O_DIRECTORY && ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY)) {
        Log::Warning("SysOpen: Not a directory! (O_DIRECTORY specified)");
        return -ENOTDIR;
    }

    if (flags & O_TRUNC && ((flags & O_ACCESS) == O_RDWR || (flags & O_ACCESS) == O_WRONLY)) {
        node->Truncate(0);
    }

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(fs::Open(node, SC_ARG1(r)));

    if (flags & O_APPEND) {
        handle->pos = handle->node->size;
    }

    return proc->AllocateHandle(std::move(handle));
}

long SysCloseHandle(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    int err = currentProcess->DestroyHandle(SC_ARG0(r));
    if (err) {
        return -EBADF;
    }

    return 0;
}

long SysSleep(RegisterContext* r) { return 0; }

long SysCreate(RegisterContext* r) {
    Log::Error("SysCreate: is a stub!");
    return -ENOSYS;
}

long SysLink(RegisterContext* r) {
    const char* oldpath = (const char*)SC_ARG0(r);
    const char* newpath = (const char*)SC_ARG1(r);

    Process* proc = Scheduler::GetCurrentProcess();

    if (!(Memory::CheckUsermodePointer(SC_ARG0(r), 1, proc->addressSpace) &&
          Memory::CheckUsermodePointer(SC_ARG1(r), 1, proc->addressSpace))) {
        Log::Warning("SysLink: Invalid path pointer");
        return -EFAULT;
    }

    FsNode* file = fs::ResolvePath(oldpath);
    if (!file) {
        Log::Warning("SysLink: Could not resolve path: %s", oldpath);
        return -ENOENT;
    }

    FsNode* parentDirectory = fs::ResolveParent(newpath, proc->workingDir->node);
    char* linkName = fs::BaseName(newpath);

    Log::Info("SysLink: Attempting to create link %s at path %s", linkName, newpath);

    if (!parentDirectory) {
        Log::Warning("SysLink: Could not resolve path: %s", newpath);
        return -ENOENT;
    }

    if ((parentDirectory->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        Log::Warning("SysLink: Could not resolve path: Parent not a directory: %s", newpath);
        return -ENOTDIR;
    }

    DirectoryEntry entry;
    strcpy(entry.name, linkName);
    return parentDirectory->Link(file, &entry);
}

long SysUnlink(RegisterContext* r) {
    int fd = SC_ARG0(r);
    const char* path = (const char*)SC_ARG1(r);
    int flag = SC_ARG2(r);

    if(flag & AT_REMOVEDIR) {
        Log::Warning("SysUnlink: AT_REMOVEDIR unsupported!");
        return -EINVAL;
    }

    if(flag & ~(AT_REMOVEDIR)) {
        return -EINVAL;
    }

    Process* proc = Scheduler::GetCurrentProcess();

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), 1, proc->addressSpace)) {
        Log::Warning("SysUnlink: Invalid path pointer");
        return -EFAULT;
    }

    FsNode* workingDir;
    if(fd == AT_FDCWD) {
        workingDir = proc->workingDir->node;
    } else {
        workingDir = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(fd))->node;
    }

    char* linkName = fs::BaseName(path);

    if (!workingDir) {
        Log::Warning("SysUnlink: Could not resolve path: %s", path);
        return -EINVAL;
    }

    if (!workingDir->IsDirectory()) {
        Log::Warning("SysUnlink: Could not resolve path: Not a directory: %s", path);
        return -ENOTDIR;
    }

    DirectoryEntry entry;
    strcpy(entry.name, linkName);
    return workingDir->Unlink(&entry);
}

long SysExecve(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    size_t filePathLength;
    long filePathInvalid =
        strlenSafe(reinterpret_cast<char*>(SC_ARG0(r)), filePathLength, currentProcess->addressSpace);
    if (filePathInvalid) {
        Log::Warning("SysExec: Reached unallocated memory reading file path");
        return -EFAULT;
    }

    char filepath[filePathLength + 1];

    strncpy(filepath, (char*)SC_ARG0(r), filePathLength);
    char** argv = (char**)SC_ARG1(r);
    char** envp = (char**)SC_ARG2(r);

    FsNode* node = fs::ResolvePath(filepath, currentProcess->workingDir->node, true /* Follow Symlinks */);
    if (!node) {
        return -ENOENT;
    }

    Vector<String> kernelEnvp;
    if (envp) {
        int i = 0;
        while (envp[i]) {
            size_t len;
            if (strlenSafe(envp[i], len, currentProcess->addressSpace)) {
                Log::Warning("SysExecve: Reached unallocated memory reading environment");
                return -EFAULT;
            }

            kernelEnvp.add_back(envp[i]);
            i++;
        }
    }

    Log::Info("Loading: %s", (char*)SC_ARG0(r));

    Vector<String> kernelArgv;
    if (argv) {
        int i = 0;
        while (argv[i]) {
            size_t len;
            if (strlenSafe(argv[i], len, currentProcess->addressSpace)) {
                Log::Warning("SysExecve: Reached unallocated memory reading argv");
                return -EFAULT;
            }

            kernelArgv.add_back(argv[i]);
            i++;
        }
    }

    if (!kernelArgv.size()) {
        kernelArgv.add_back(filepath); // Ensure at least argv[0] is set
    }

    timeval tv = Timer::GetSystemUptimeStruct();
    uint8_t* buffer = (uint8_t*)kmalloc(node->size);
    size_t read = fs::Read(node, 0, node->size, buffer);
    if (read != node->size) {
        Log::Warning("Could not read file: %s", filepath);
        return -EIO;
    }
    timeval tvnew = Timer::GetSystemUptimeStruct();
    Log::Info("Done (took %d us)", Timer::TimeDifference(tvnew, tv));

    Thread* currentThread = Scheduler::GetCurrentThread();
    ScopedSpinLock lockProcess(currentProcess->m_processLock);

    currentProcess->addressSpace->UnmapAll();
    currentProcess->usedMemoryBlocks = 0;

    currentProcess->MapSignalTrampoline();

    // Reset register state
    memset(r, 0, sizeof(RegisterContext));

    MappedRegion* stackRegion =
        currentProcess->addressSpace->AllocateAnonymousVMObject(0x200000, 0, false); // 2MB max stacksize
    currentThread->stack = reinterpret_cast<void*>(stackRegion->base);               // 256KB stack size
    r->rsp = (uintptr_t)currentThread->stack + 0x200000;

    // Force the first 8KB to be allocated
    stackRegion->vmObject->Hit(stackRegion->base, 0x200000 - 0x1000, currentProcess->GetPageMap());
    stackRegion->vmObject->Hit(stackRegion->base, 0x200000 - 0x2000, currentProcess->GetPageMap());

    elf_info_t elfInfo = LoadELFSegments(currentProcess, buffer, 0);
    r->rip = currentProcess->LoadELF(&r->rsp, elfInfo, kernelArgv, kernelEnvp, filepath);
    kfree(buffer);

    if (!r->rip) {
        // Its really important that we kill the process afterwards,
        // Otherwise the process pointer reference counter will not be decremeneted,
        // and the ScopedSpinlocks will not be released.
        currentThread->Signal(SIGKILL);
        return -1;
    }

    strncpy(currentProcess->name, kernelArgv[0].c_str(), sizeof(currentProcess->name));

    assert(!(r->rsp & 0xF));

    r->cs = USER_CS;
    r->ss = USER_SS;

    r->rbp = r->rsp;
    r->rflags = 0x202; // IF - Interrupt Flag, bit 1 should be 1
    memset(currentThread->fxState, 0, 4096);

    ((fx_state_t*)currentThread->fxState)->mxcsr = 0x1f80; // Default MXCSR (SSE Control Word) State
    ((fx_state_t*)currentThread->fxState)->mxcsrMask = 0xffbf;
    ((fx_state_t*)currentThread->fxState)->fcw = 0x33f; // Default FPU Control Word State

    // Restore default FPU state
    asm volatile("fxrstor64 (%0)" ::"r"((uintptr_t)currentThread->fxState) : "memory");

    ScopedSpinLock lockProcessFds(currentProcess->m_handleLock);
    for (Handle& fd : currentProcess->m_handles) {
        if (fd.closeOnExec) {
            fd = HANDLE_NULL;
        }
    }

    return 0;
}

long SysChdir(RegisterContext* r) {
    Process* proc = Process::Current();
    if (SC_ARG0(r)) {
        char* path = fs::CanonicalizePath((char*)SC_ARG0(r), proc->workingDirPath);
        FsNode* n = fs::ResolvePath(path);
        if (!n) {
            Log::Warning("chdir: Could not find %s", path);
            return -ENOENT;
        } else if (!n->IsDirectory()) {
            return -ENOTDIR;
        }

        strcpy(proc->workingDirPath, path);
        proc->workingDir = SC_TRY_OR_ERROR(n->Open(0));
    } else
        Log::Warning("chdir: Invalid path string");
    return 0;
}

long SysTime(RegisterContext* r) { return 0; }

long SysMapFB(RegisterContext* r) {
    video_mode_t vMode = Video::GetVideoMode();
    Process* process = Scheduler::GetCurrentProcess();

    MappedRegion* region = nullptr;
    region = process->addressSpace->MapVMO(Video::GetFramebufferVMO(), 0, false);

    if (!region || !region->Base()) {
        return -1;
    }

    uintptr_t fbVirt = region->Base();

    fb_info_t fbInfo;
    fbInfo.width = vMode.width;
    fbInfo.height = vMode.height;
    fbInfo.bpp = vMode.bpp;
    fbInfo.pitch = vMode.pitch;

    if (HAL::debugMode)
        fbInfo.height = vMode.height / 3 * 2;

    UserPointer<uintptr_t> virt = SC_ARG0(r);
    UserPointer<fb_info_t> info = SC_ARG1(r);

    TRY_STORE_UMODE_VALUE(virt, fbVirt);
    TRY_STORE_UMODE_VALUE(info, fbInfo);

    Log::Info("fb mapped at %x", region->Base());

    return 0;
}

/////////////////////////////
/// \name SysGetTID - Get thread ID
///
/// \return Current thread's ID
/////////////////////////////
long SysGetTID(RegisterContext* r) { return Scheduler::GetCurrentThread()->tid; }

long SysChmod(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();

    const char* path = reinterpret_cast<const char*>(SC_ARG0(r));
    mode_t mode = SC_ARG1(r);

    size_t pathLen = 0;
    if (strlenSafe(path, pathLen, proc->addressSpace)) {
        return -EFAULT;
    }

    if (pathLen > PATH_MAX) {
        return -ENAMETOOLONG;
    }

    char tempPath[pathLen + 1];
    strcpy(tempPath, path);

    FsNode* file = fs::ResolvePath(tempPath, proc->workingDir->node);
    if (!file) {
        return -ENOENT;
    }

    Log::Warning("SysChmod: chmod is a stub!");

    (void)mode;

    return 0;
}

long SysFStat(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    stat_t* stat = (stat_t*)SC_ARG0(r);
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(process->GetHandleAs<UNIXOpenFile>(SC_ARG1(r)));
    if (!handle) {
        Log::Warning("sys_fstat: Invalid File Descriptor, %d", SC_ARG1(r));
        return -EBADF;
    }

    FsNode* const& node = handle->node;

    stat->st_dev = 0;
    stat->st_ino = node->inode;
    stat->st_mode = 0;

    if ((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY)
        stat->st_mode |= S_IFDIR;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_FILE)
        stat->st_mode |= S_IFREG;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_BLKDEVICE)
        stat->st_mode |= S_IFBLK;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_CHARDEVICE)
        stat->st_mode |= S_IFCHR;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK)
        stat->st_mode |= S_IFLNK;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_SOCKET)
        stat->st_mode |= S_IFSOCK;

    stat->st_nlink = 0;
    stat->st_uid = node->uid;
    stat->st_gid = 0;
    stat->st_rdev = 0;
    stat->st_size = node->size;
    stat->st_blksize = 0;
    stat->st_blocks = 0;

    return 0;
}

long SysStat(RegisterContext* r) {
    stat_t* stat = (stat_t*)SC_ARG0(r);
    char* filepath = (char*)SC_ARG1(r);
    uint64_t flags = SC_ARG2(r);
    Process* proc = Scheduler::GetCurrentProcess();

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), sizeof(stat_t), proc->addressSpace)) {
        Log::Warning("SysStat: stat structure points to invalid address %x", SC_ARG0(r));
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), 1, proc->addressSpace)) {
        Log::Warning("SysStat: filepath points to invalid address %x", SC_ARG0(r));
    }

    bool followSymlinks = !(flags & AT_SYMLINK_NOFOLLOW);
    FsNode* node = fs::ResolvePath(filepath, proc->workingDir->node, followSymlinks);

    if (!node) {
        Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "SysStat: Invalid filepath %s", filepath);
        return -ENOENT;
    }

    stat->st_dev = 0;
    stat->st_ino = node->inode;
    stat->st_mode = 0;

    if ((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY)
        stat->st_mode = S_IFDIR;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_FILE)
        stat->st_mode = S_IFREG;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_BLKDEVICE)
        stat->st_mode = S_IFBLK;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_CHARDEVICE)
        stat->st_mode = S_IFCHR;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK)
        stat->st_mode = S_IFLNK;
    if ((node->flags & FS_NODE_TYPE) == FS_NODE_SOCKET)
        stat->st_mode = S_IFSOCK;

    stat->st_nlink = 0;
    stat->st_uid = node->uid;
    stat->st_gid = 0;
    stat->st_rdev = 0;
    stat->st_size = node->size;
    stat->st_blksize = 0;
    stat->st_blocks = 0;

    return 0;
}

long SysLSeek(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(process->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysLSeek: Invalid File Descriptor, %d", SC_ARG0(r));
        return -EBADF;
    }

    switch (SC_ARG2(r)) {
    case 0: // SEEK_SET
        return (handle->pos = SC_ARG1(r));
        break;
    case 1: // SEEK_CUR
        return handle->pos;
    case 2: // SEEK_END
        return (handle->pos = handle->node->size);
        break;
    default:
        Log::Warning("SysLSeek: Invalid seek: %d, mode: %d", SC_ARG0(r), SC_ARG2(r));
        return -EINVAL; // Invalid seek mode
    }
}

long SysGetPID(RegisterContext* r) {
    uint64_t* pid = (uint64_t*)SC_ARG0(r);

    *pid = Scheduler::GetCurrentProcess()->PID();

    return 0;
}

long SysMount(RegisterContext* r) { return 0; }

long SysMkdir(RegisterContext* r) {
    char* path = (char*)SC_ARG0(r);
    mode_t mode = SC_ARG1(r);

    Process* proc = Scheduler::GetCurrentProcess();

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), 0, proc->addressSpace)) {
        Log::Warning("sys_mkdir: Invalid path pointer %x", SC_ARG0(r));
        return -EFAULT;
    }

    FsNode* parentDirectory = fs::ResolveParent(path, proc->workingDir->node);
    char* dirPath = fs::BaseName(path);

    Log::Info("sys_mkdir: Attempting to create %s at path %s", dirPath, path);

    if (!parentDirectory) {
        Log::Warning("sys_mkdir: Could not resolve path: %s", path);
        return -EINVAL;
    }

    if ((parentDirectory->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        Log::Warning("sys_mkdir: Could not resolve path: Not a directory: %s", path);
        return -ENOTDIR;
    }

    DirectoryEntry dir;
    strcpy(dir.name, dirPath);
    int ret = parentDirectory->CreateDirectory(&dir, mode);

    return ret;
}

long SysRmdir(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    if (!Memory::CheckUsermodePointer(SC_ARG0(r), 1, proc->addressSpace)) {
        return -EFAULT;
    }

    char* path = reinterpret_cast<char*>(SC_ARG0(r));
    FsNode* node = fs::ResolvePath(path, proc->workingDir->node);
    if (!node) {
        return -ENOENT;
    }

    if ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        return -ENOTDIR;
    }

    DirectoryEntry ent;
    if (node->ReadDir(&ent, 0)) {
        return -ENOTEMPTY;
    }

    FsNode* parent = fs::ResolveParent(path);
    strcpy(ent.name, fs::BaseName(path));
    ent.inode = node->inode;

    fs::Unlink(parent, &ent, true);
    return 0;
}

long SysRename(RegisterContext* r) {
    char* oldpath = (char*)SC_ARG0(r);
    char* newpath = (char*)SC_ARG1(r);

    Process* proc = Scheduler::GetCurrentProcess();

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), 0, proc->addressSpace)) {
        Log::Warning("sys_rename: Invalid oldpath pointer %x", SC_ARG0(r));
        return -EFAULT;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), 0, proc->addressSpace)) {
        Log::Warning("sys_rename: Invalid newpath pointer %x", SC_ARG0(r));
        return -EFAULT;
    }

    FsNode* olddir = fs::ResolveParent(oldpath, proc->workingDir->node);
    FsNode* newdir = fs::ResolveParent(newpath, proc->workingDir->node);

    if (!(olddir && newdir)) {
        return -ENOENT;
    }

    return fs::Rename(olddir, fs::BaseName(oldpath), newdir, fs::BaseName(newpath));
}

long SysYield(RegisterContext* r) {
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
long SysReadDirNext(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(process->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        return -EBADF;
    }

    fs_dirent_t* direntPointer = (fs_dirent_t*)SC_ARG1(r);
    if (!Memory::CheckUsermodePointer((uintptr_t)direntPointer, sizeof(fs_dirent_t),
                                      Scheduler::GetCurrentProcess()->addressSpace)) {
        return -EFAULT;
    }

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        return -ENOTDIR;
    }

    DirectoryEntry tempent;
    int ret = fs::ReadDir(handle, &tempent, handle->pos++);

    strcpy(direntPointer->name, tempent.name);
    direntPointer->type = tempent.flags;
    direntPointer->inode = tempent.inode;

    return ret;
}

long SysRenameAt(RegisterContext* r) {
    Log::Warning("SysRenameAt is a stub!");
    return -ENOSYS;
}

// SendMessage(message_t* msg) - Sends an IPC message to a process
long SysSendMessage(RegisterContext* r) {
    Log::Warning("SysSendMessage is a stub!");
    return -ENOSYS;
}

// RecieveMessage(message_t* msg) - Grabs next message on queue and copies it to msg
long SysReceiveMessage(RegisterContext* r) {
    Log::Warning("SysReceiveMessage is a stub!");
    return -ENOSYS;
}

long SysUptime(RegisterContext* r) {
    uint64_t* seconds = (uint64_t*)SC_ARG0(r);
    uint64_t* milliseconds = (uint64_t*)SC_ARG1(r);
    if (seconds) {
        *seconds = Timer::GetSystemUptime();
    }
    if (milliseconds) {
        *milliseconds = Timer::UsecondsSinceBoot() / 1000;
    }
    return 0;
}

long SysDebug(RegisterContext* r) {
    Log::Info("(%s): %s, %d", Scheduler::GetCurrentProcess()->name, (char*)SC_ARG0(r), SC_ARG1(r));
    return 0;
}

long SysGetVideoMode(RegisterContext* r) {
    video_mode_t vMode = Video::GetVideoMode();
    fb_info_t fbInfo;
    fbInfo.width = vMode.width;
    fbInfo.height = vMode.height;
    if (HAL::debugMode)
        fbInfo.height = vMode.height / 3 * 2;
    fbInfo.bpp = vMode.bpp;
    fbInfo.pitch = vMode.pitch;

    *((fb_info_t*)SC_ARG0(r)) = fbInfo;

    return 0;
}

long SysUName(RegisterContext* r) {
    if (Memory::CheckUsermodePointer(SC_ARG0(r), strlen(Lemon::versionString),
                                     Scheduler::GetCurrentProcess()->addressSpace)) {
        strcpy((char*)SC_ARG0(r), Lemon::versionString);
        return 0;
    }

    return -EFAULT;
}

long SysReadDir(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(process->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        return -EBADF;
    }

    fs_dirent_t* direntPointer = (fs_dirent_t*)SC_ARG1(r);
    if (!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(fs_dirent_t), Scheduler::GetCurrentProcess()->addressSpace)) {
        return -EFAULT;
    }

    unsigned int count = SC_ARG2(r);

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        return -ENOTDIR;
    }

    DirectoryEntry tempent;
    int ret = fs::ReadDir(handle, &tempent, count);

    strcpy(direntPointer->name, tempent.name);
    direntPointer->type = tempent.flags;

    return ret;
}

long SysSetFsBase(RegisterContext* r) {
    asm volatile("wrmsr" ::"a"(SC_ARG0(r) & 0xFFFFFFFF) /*Value low*/,
                 "d"((SC_ARG0(r) >> 32) & 0xFFFFFFFF) /*Value high*/, "c"(0xC0000100) /*Set FS Base*/);
    GetCPULocal()->currentThread->fsBase = SC_ARG0(r);
    return 0;
}

long SysMmap(RegisterContext* r) {
    uint64_t* address = (uint64_t*)SC_ARG0(r);
    size_t size = SC_ARG1(r);
    uintptr_t hint = SC_ARG2(r);
    uint64_t flags = SC_ARG3(r);

    if (!size) {
        return -EINVAL; // We do not accept 0-length mappings
    }

    Process* proc = Scheduler::GetCurrentProcess();

    bool fixed = flags & MAP_FIXED;
    bool anon = flags & MAP_ANON;
    // bool privateMapping = flags & MAP_PRIVATE;

    uint64_t unknownFlags = flags & ~static_cast<uint64_t>(MAP_ANON | MAP_FIXED | MAP_PRIVATE);
    if (unknownFlags || !anon) {
        Log::Warning("SysMmap: Unsupported mmap flags %x", flags);
        return -EINVAL;
    }

    if (size > 0x40000000) { // size > 1GB
        Log::Warning("MMap size: %u (>1GB)", size);
        Log::Info("rip: %x", r->rip);
        UserPrintStackTrace(r->rbp, proc->addressSpace);
        return -EINVAL;
    }

    MappedRegion* region = proc->addressSpace->AllocateAnonymousVMObject(size, hint, fixed);
    if (!region || !region->base) {
        IF_DEBUG((debugLevelSyscalls >= DebugLevelNormal), {
            Log::Error("SysMmap: Failed to map region (hint %x)!", hint);
            Log::Info("rip: %x", r->rip);
            UserPrintStackTrace(r->rbp, proc->addressSpace);
        });

        return -1;
    }

    *address = region->base;
    return 0;
}

long SysGetCWD(RegisterContext* r) {
    char* buf = (char*)SC_ARG0(r);
    size_t sz = SC_ARG1(r);

    char* workingDir = Scheduler::GetCurrentProcess()->workingDirPath;
    if (strlen(workingDir) > sz) {
        return 1;
    } else {
        strcpy(buf, workingDir);
        return 0;
    }

    return 0;
}

long SysWaitPID(RegisterContext* r) {
    int pid = SC_ARG0(r);
    int* status = reinterpret_cast<int*>(SC_ARG1(r));
    int64_t flags = SC_ARG2(r);

    Process* currentProcess = Scheduler::GetCurrentProcess();
    FancyRefPtr<Process> child = nullptr;

    if (pid == -1) {
        child = currentProcess->RemoveDeadChild();
        if (child.get()) {
            return child->PID();
        }

        if (flags & WNOHANG) {
            return 0;
        }

        if (int e = currentProcess->WaitForChildToDie(child); e) {
            return e; // Error waiting for a child to die
        }

        assert(child.get());
        if (status) {
            *status = child->exitCode;
        }

        return child->PID();
    }

    if ((child = currentProcess->FindChildByPID(pid)).get()) {
        if (child->IsDead()) {
            pid = child->PID();
            currentProcess->RemoveDeadChild(child->PID());
            return pid;
        }

        if (flags & WNOHANG) {
            return 0;
        }

        pid = 0;
        KernelObjectWatcher bl;

        bl.WatchObject(static_pointer_cast<KernelObject>(child), 0);

        if (bl.Wait()) {
            if (child->IsDead()) { // Could have been SIGCHLD signal
                currentProcess->RemoveDeadChild(child->PID());
                return child->PID();
            } else {
                return -EINTR;
            }
        }

        assert(child->IsDead());
        if (status) {
            *status = child->exitCode;
        }

        currentProcess->RemoveDeadChild(child->PID());

        return child->PID();
    }

    return -ECHILD;
}

long SysNanoSleep(RegisterContext* r) {
    uint64_t nanoseconds = SC_ARG0(r);

    Scheduler::GetCurrentThread()->Sleep(nanoseconds / 1000);

    return 0;
}

long SysPRead(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(currentProcess->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysPRead: Invalid file descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), SC_ARG2(r), currentProcess->addressSpace)) {
        return -EFAULT;
    }

    uint8_t* buffer = (uint8_t*)SC_ARG1(r);
    uint64_t count = SC_ARG2(r);
    uint64_t off = SC_ARG4(r);
    return fs::Read(handle->node, off, count, buffer);
}

long SysPWrite(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(currentProcess->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysPRead: Invalid file descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), SC_ARG2(r), currentProcess->addressSpace)) {
        return -EFAULT;
    }

    uint8_t* buffer = (uint8_t*)SC_ARG1(r);
    uint64_t count = SC_ARG2(r);
    uint64_t off = SC_ARG4(r);
    return fs::Read(handle->node, off, count, buffer);
}

long SysIoctl(RegisterContext* r) {
    uint64_t request = SC_ARG1(r);
    uint64_t arg = SC_ARG2(r);
    int* result = (int*)SC_ARG3(r);

    Process* process = Scheduler::GetCurrentProcess();

    if (result && !Memory::CheckUsermodePointer((uintptr_t)result, sizeof(int), process->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
            Log::Warning("(%s): SysIoctl: Invalid result value %x", process->name, SC_ARG0(r));
            Log::Info("%x", r->rip);
            UserPrintStackTrace(r->rbp, process->addressSpace);
        });
        return -EFAULT;
    }

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(process->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysIoctl: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    if (request == FIOCLEX) {
        handle->mode |= O_CLOEXEC;
        return 0;
    }

    int ret = fs::Ioctl(handle, request, arg);

    if (result && ret > 0) {
        *result = ret;
    }

    return ret;
}

long SysInfo(RegisterContext* r) {
    lemon_sysinfo_t* s = (lemon_sysinfo_t*)SC_ARG0(r);

    if (!s) {
        return -EFAULT;
    }

    s->usedMem = Memory::usedPhysicalBlocks * 4;
    s->totalMem = HAL::mem_info.totalMemory / 1024;
    s->cpuCount = static_cast<uint16_t>(SMP::processorCount);

    return 0;
}

/*
 * SysMunmap - Unmap memory (addr, size)
 *
 * On success - return 0
 * On failure - return -1
 */
long SysMunmap(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    uint64_t address = SC_ARG0(r);
    size_t size = SC_ARG1(r);

    if (address & (PAGE_SIZE_4K - 1) || size & (PAGE_SIZE_4K - 1)) {
        return -EINVAL; // Must be aligned
    }

    return process->addressSpace->UnmapMemory(address, size);
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
long SysCreateSharedMemory(RegisterContext* r) {
    int64_t* key = (int64_t*)SC_ARG0(r);
    uint64_t size = SC_ARG1(r);
    uint64_t flags = SC_ARG2(r);
    uint64_t recipient = SC_ARG3(r);

    *key = Memory::CreateSharedMemory(size, flags, Scheduler::GetCurrentProcess()->PID(), recipient);
    assert(*key);

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
long SysMapSharedMemory(RegisterContext* r) {
    void** ptr = (void**)SC_ARG0(r);
    int64_t key = SC_ARG1(r);
    uint64_t hint = SC_ARG2(r);

    *ptr = Memory::MapSharedMemory(key, Scheduler::GetCurrentProcess(), hint);

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
long SysUnmapSharedMemory(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();

    uint64_t address = SC_ARG0(r);
    int64_t key = SC_ARG1(r);

    {
        FancyRefPtr<SharedVMObject> sMem = Memory::GetSharedMemory(key);
        if (!sMem.get())
            return -EINVAL;

        MappedRegion* region = proc->addressSpace->AddressToRegionWriteLock(address);
        if (!region || region->vmObject != sMem) {
            region->lock.ReleaseWrite();
            return -EINVAL; // Invalid memory region
        }

        long r = proc->addressSpace->UnmapRegion(region);
        assert(!r);
    }

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
long SysDestroySharedMemory(RegisterContext* r) {
    uint64_t key = SC_ARG0(r);

    if (Memory::CanModifySharedMemory(Scheduler::GetCurrentProcess()->PID(), key)) {
        Memory::DestroySharedMemory(key);
    } else
        return -EPERM;

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
long SysSocket(RegisterContext* r) {
    int domain = SC_ARG0(r);
    int type = SC_ARG1(r);
    int protocol = SC_ARG2(r);

    Socket* sock = nullptr;
    int e = Socket::CreateSocket(domain, type, protocol, &sock);

    if (e < 0)
        return e;
    assert(sock);

    UNIXOpenFile* fDesc = SC_TRY_OR_ERROR(fs::Open(sock, 0));

    if (type & SOCK_NONBLOCK)
        fDesc->mode |= O_NONBLOCK;

    return Scheduler::GetCurrentProcess()->AllocateHandle(fDesc);
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
long SysBind(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysBind: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        Log::Warning("SysBind: File (Descriptor: %d) is not a socket", SC_ARG0(r));
        return -ENOTSOCK;
    }

    socklen_t len = SC_ARG2(r);

    sockaddr_t* addr = (sockaddr_t*)SC_ARG1(r);
    if (!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)) {
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
long SysListen(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysListen: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        Log::Warning("SysListen: File (Descriptor: %d) is not a socket", SC_ARG0(r));
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
long SysAccept(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysAccept: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        Log::Warning("SysAccept: File (Descriptor: %d) is not a socket", SC_ARG0(r));
        return -ENOTSOCK;
    }

    socklen_t* len = (socklen_t*)SC_ARG2(r);
    if (len && !Memory::CheckUsermodePointer(SC_ARG2(r), sizeof(socklen_t), proc->addressSpace)) {
        Log::Warning("SysAccept: Invalid socklen ptr");
        return -EFAULT;
    }

    sockaddr_t* addr = (sockaddr_t*)SC_ARG1(r);
    if (addr && !Memory::CheckUsermodePointer(SC_ARG1(r), *len, proc->addressSpace)) {
        Log::Warning("SysAccept: Invalid sockaddr ptr");
        return -EFAULT;
    }

    Socket* sock = (Socket*)handle->node;

    Socket* newSock = sock->Accept(addr, len, handle->mode);
    if (!newSock) {
        return -EAGAIN;
    }

    UNIXOpenFile* newHandle = SC_TRY_OR_ERROR(fs::Open(newSock));
    return proc->AllocateHandle(newHandle);
}

/////////////////////////////
/// \name SysConnect (sockfd, addr, addrlen) - Initiate socket connection
/// \param sockfd - Socket file descriptor
/// \param addr - sockaddr structure
/// \param addrlen - size of addr
///
/// \return 0 on success or negative error code on failure
/////////////////////////////
long SysConnect(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysConnect: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        Log::Warning("sys_connect: File (Descriptor: %d) is not a socket", SC_ARG0(r));
        return -EFAULT;
    }

    socklen_t len = SC_ARG2(r);

    sockaddr_t* addr = (sockaddr_t*)SC_ARG1(r);
    if (!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)) {
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
long SysSend(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysSend: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    uint8_t* buffer = (uint8_t*)(SC_ARG1(r));
    size_t len = SC_ARG2(r);
    uint64_t flags = SC_ARG3(r);

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        Log::Warning("SysSend: File (Descriptor: %d) is not a socket", SC_ARG0(r));
        return -ENOTSOCK;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)) {
        Log::Warning("SysSend: Invalid buffer ptr");
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
long SysSendTo(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysSendTo: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    uint8_t* buffer = (uint8_t*)(SC_ARG1(r));
    size_t len = SC_ARG2(r);
    uint64_t flags = SC_ARG3(r);

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        Log::Warning("SysSendTo: File (Descriptor: %d) is not a socket", SC_ARG0(r));
        return -ENOTSOCK;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)) {
        Log::Warning("SysSendTo: Invalid buffer ptr");
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
long SysReceive(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysReceive: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    uint8_t* buffer = (uint8_t*)(SC_ARG1(r));
    size_t len = SC_ARG2(r);
    uint64_t flags = SC_ARG3(r);

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        Log::Warning("SysReceive: File (Descriptor: %d) is not a socket", SC_ARG0(r));
        return -ENOTSOCK;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)) {
        Log::Warning("SysReceive: Invalid buffer ptr");
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
long SysReceiveFrom(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysReceiveFrom: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    uint8_t* buffer = (uint8_t*)(SC_ARG1(r));
    size_t len = SC_ARG2(r);
    uint64_t flags = SC_ARG3(r);

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        Log::Warning("SysReceiveFrom: File (Descriptor: %d) is not a socket", SC_ARG0(r));
        return -ENOTSOCK;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), len, proc->addressSpace)) {
        Log::Warning("SysReceiveFrom: Invalid buffer ptr");
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
long SysGetUID(RegisterContext* r) { return Scheduler::GetCurrentProcess()->uid; }

/*
 * SetSetUID () - Set Process UID
 *
 * On Success - Return process UID
 * On Failure - Return negative value
 */
long SysSetUID(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    uid_t requestedUID = SC_ARG0(r);

    if (proc->uid == requestedUID) {
        return 0;
    }

    if (proc->euid == 0) {
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
 * timeout - timeout period (in ms)
 *
 * On Success - return number of file descriptors
 * On Failure - return -1
 */
long SysPoll(RegisterContext* r) {
    pollfd* fds = (pollfd*)SC_ARG0(r);
    unsigned nfds = SC_ARG1(r);
    long timeout = SC_ARG2(r);

    Thread* thread = GetCPULocal()->currentThread;
    Process* proc = Scheduler::GetCurrentProcess();

    if (!nfds) {
        thread->Sleep(timeout * 1000); // Caller must have been using poll to wait/sleep
        return 0;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), nfds * sizeof(pollfd), proc->addressSpace)) {
        Log::Warning("SysPoll: Invalid pointer to file descriptor array");
        IF_DEBUG(debugLevelSyscalls >= DebugLevelVerbose, {
            Log::Info("rip: %x", r->rip);
            UserPrintStackTrace(r->rbp, Scheduler::GetCurrentProcess()->addressSpace);
        });
        return -EFAULT;
    }

    FancyRefPtr<UNIXOpenFile> files[nfds];

    unsigned eventCount = 0; // Amount of fds with events
    for (unsigned i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) {
            Log::Warning("SysPoll: Invalid File Descriptor: %d", fds[i].fd);
            files[i] = nullptr;
            fds[i].revents |= POLLNVAL;
            eventCount++;
            continue;
        }

        auto result = Scheduler::GetCurrentProcess()->GetHandleAs<UNIXOpenFile>(fds[i].fd);
        if (result.HasError()) {
            Log::Warning("SysPoll: Invalid File Descriptor: %d", fds[i].fd);
            files[i] = nullptr;
            fds[i].revents |= POLLNVAL;
            eventCount++;
            continue;
        }

        files[i] = std::move(result.Value());

        bool hasEvent = 0;

        if ((files[i]->node->flags & FS_NODE_TYPE) == FS_NODE_SOCKET) {
            if (!((Socket*)files[i]->node)->IsConnected() && !((Socket*)files[i]->node)->IsListening()) {
                Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "%s: SysPoll: fd %d disconnected!", proc->name,
                           fds[i].fd);
                fds[i].revents |= POLLHUP;
                hasEvent = true;
            }

            if (((Socket*)files[i]->node)->PendingConnections() && (fds[i].events & POLLIN)) {
                Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "%s: SysPoll: fd %d has pending connections!",
                           proc->name, fds[i].fd);
                fds[i].revents |= POLLIN;
                hasEvent = true;
            }
        }

        if (files[i]->node->CanRead() && (fds[i].events & POLLIN)) { // If readable and the caller requested POLLIN
            Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "%s: SysPoll: fd %d readable!", proc->name, fds[i].fd);
            fds[i].revents |= POLLIN;
            hasEvent = true;
        }

        if (files[i]->node->CanWrite() && (fds[i].events & POLLOUT)) { // If writable and the caller requested POLLOUT
            Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "%s: SysPoll: fd %d writable!", proc->name, fds[i].fd);
            fds[i].revents |= POLLOUT;
            hasEvent = true;
        }

        if (hasEvent)
            eventCount++;
    }

    if (!eventCount && timeout) {
        timeval tVal = Timer::GetSystemUptimeStruct();

        FilesystemWatcher fsWatcher;
        for (unsigned i = 0; i < nfds; i++) {
            if (files[i])
                fsWatcher.WatchNode(files[i]->node, fds[i].events);
        }

        if (timeout > 0) {
            if (fsWatcher.WaitTimeout(timeout)) {
                return -EINTR; // Interrupted
            } else if (timeout <= 0) {
                return 0; // Timed out
            }
        } else if (fsWatcher.Wait()) {
            return -EINTR; // Interrupted
        }

        do {
            if (eventCount > 0) {
                break;
            }

            for (unsigned i = 0; i < nfds; i++) {
                if (!files[i] || !files[i]->node)
                    continue;

                bool hasEvent = 0;
                if ((files[i]->node->flags & FS_NODE_TYPE) == FS_NODE_SOCKET) {
                    if (!((Socket*)files[i]->node)->IsConnected() && !((Socket*)files[i]->node)->IsListening()) {
                        fds[i].revents |= POLLHUP;
                        hasEvent = 1;
                    }

                    if (((Socket*)files[i]->node)->PendingConnections() && (fds[i].events & POLLIN)) {
                        fds[i].revents |= POLLIN;
                        hasEvent = 1;
                    }
                }

                if (files[i]->node->CanRead() &&
                    (fds[i].events & POLLIN)) { // If readable and the caller requested POLLIN
                    fds[i].revents |= POLLIN;
                    hasEvent = 1;
                }

                if (files[i]->node->CanWrite() &&
                    (fds[i].events & POLLOUT)) { // If writable and the caller requested POLLOUT
                    fds[i].revents |= POLLOUT;
                    hasEvent = 1;
                }

                if (hasEvent)
                    eventCount++;
            }

            if (eventCount) {
                Scheduler::Yield();
            }
        } while (
            thread->state != ThreadStateZombie &&
            (timeout < 0 || Timer::TimeDifference(Timer::GetSystemUptimeStruct(), tVal) <
                                timeout)); // Wait until timeout, unless timeout is negative in which wait infinitely
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
long SysSendMsg(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal,
                 { Log::Warning("SysSendMsg: Invalid File Descriptor: %d", SC_ARG0(r)); });
        return -EBADF;
    }

    msghdr* msg = (msghdr*)SC_ARG1(r);
    uint64_t flags = SC_ARG3(r);

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {

        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal,
                 { Log::Warning("SysSendMsg: File (Descriptor: %d) is not a socket", SC_ARG0(r)); });
        return -ENOTSOCK;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(msghdr), proc->addressSpace)) {

        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, { Log::Warning("SysSendMsg: Invalid msg ptr"); });
        return -EFAULT;
    }

    if (!Memory::CheckUsermodePointer((uintptr_t)msg->msg_iov, sizeof(iovec) * msg->msg_iovlen, proc->addressSpace)) {

        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, { Log::Warning("SysSendMsg: msg: Invalid iovec ptr"); });
        return -EFAULT;
    }

    if (msg->msg_name && msg->msg_namelen &&
        !Memory::CheckUsermodePointer((uintptr_t)msg->msg_name, msg->msg_namelen, proc->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal,
                 { Log::Warning("SysSendMsg: msg: Invalid name ptr and name not null"); });
        return -EFAULT;
    }

    if (msg->msg_control && msg->msg_controllen &&
        !Memory::CheckUsermodePointer((uintptr_t)msg->msg_control, msg->msg_controllen, proc->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal,
                 { Log::Warning("SysSendMsg: msg: Invalid control ptr and control null"); });
        return -EFAULT;
    }

    long sent = 0;
    Socket* sock = (Socket*)handle->node;

    for (unsigned i = 0; i < msg->msg_iovlen; i++) {
        if (!Memory::CheckUsermodePointer((uintptr_t)msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len,
                                          proc->addressSpace)) {
            Log::Warning("SysSendMsg: msg: Invalid iovec entry base");
            return -EFAULT;
        }

        long ret = sock->SendTo(msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len, flags, (sockaddr*)msg->msg_name,
                                msg->msg_namelen, msg->msg_control, msg->msg_controllen);

        if (ret < 0)
            return ret;

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
long SysRecvMsg(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal,
                 { Log::Warning("SysRecvMsg: Invalid File Descriptor: %d", SC_ARG0(r)); });
        return -EBADF;
    }

    msghdr* msg = (msghdr*)SC_ARG1(r);
    uint64_t flags = SC_ARG3(r);

    if (handle->mode & O_NONBLOCK) {
        flags |= MSG_DONTWAIT; // Don't wait if socket marked as nonblock
    }

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal,
                 { Log::Warning("SysRecvMsg: File (Descriptor: %d) is not a socket", SC_ARG0(r)); });
        return -ENOTSOCK;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(msghdr), proc->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, { Log::Warning("SysRecvMsg: Invalid msg ptr"); });
        return -EFAULT;
    }

    if (!Memory::CheckUsermodePointer((uintptr_t)msg->msg_iov, sizeof(iovec) * msg->msg_iovlen, proc->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, { Log::Warning("SysRecvMsg: msg: Invalid iovec ptr"); });
        return -EFAULT;
    }

    if (msg->msg_control && msg->msg_controllen &&
        !Memory::CheckUsermodePointer((uintptr_t)msg->msg_control, msg->msg_controllen, proc->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal,
                 { Log::Warning("SysRecvMsg: msg: Invalid control ptr and control null"); });
        return -EFAULT;
    }

    long read = 0;
    Socket* sock = (Socket*)handle->node;

    for (unsigned i = 0; i < msg->msg_iovlen; i++) {
        if (!Memory::CheckUsermodePointer((uintptr_t)msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len,
                                          proc->addressSpace)) {
            Log::Warning("SysRecvMsg: msg: Invalid iovec entry base");
            return -EFAULT;
        }

        socklen_t len = msg->msg_namelen;
        long ret =
            sock->ReceiveFrom(msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len, flags,
                              reinterpret_cast<sockaddr*>(msg->msg_name), &len, msg->msg_control, msg->msg_controllen);
        msg->msg_namelen = len;

        if (ret < 0) {
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
long SysGetEUID(RegisterContext* r) { return Scheduler::GetCurrentProcess()->euid; }

/////////////////////////////
/// \brief SysSetEUID () - Set effective process UID
///
/// \return On success return 0, otherwise return negative error code
/////////////////////////////
long SysSetEUID(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    uid_t requestedEUID = SC_ARG0(r);

    // Unprivileged processes may only set the effective user ID to the real user ID or the effective user ID
    if (proc->euid == requestedEUID) {
        return 0;
    }

    if (proc->uid == 0 || proc->uid == requestedEUID) {
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
long SysGetProcessInfo(RegisterContext* r) {
    uint64_t pid = SC_ARG0(r);
    lemon_process_info_t* pInfo = reinterpret_cast<lemon_process_info_t*>(SC_ARG1(r));

    Process* cProcess = Scheduler::GetCurrentProcess();
    if (!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(lemon_process_info_t), cProcess->addressSpace)) {
        return -EFAULT;
    }

    FancyRefPtr<Process> reqProcess;
    if (!(reqProcess = Scheduler::FindProcessByPID(pid)).get()) {
        return -EINVAL;
    }

    pInfo->pid = pid;

    pInfo->threadCount = reqProcess->Threads().get_length();

    pInfo->uid = reqProcess->uid;
    pInfo->gid = reqProcess->gid;

    pInfo->state = reqProcess->GetMainThread()->state;

    strcpy(pInfo->name, reqProcess->name);

    pInfo->runningTime = Timer::GetSystemUptime() - reqProcess->creationTime.tv_sec;
    pInfo->activeUs = reqProcess->activeTicks * 1000000 / Timer::GetFrequency();

    pInfo->usedMem = reqProcess->addressSpace->UsedPhysicalMemory();
    pInfo->isCPUIdle = reqProcess->IsCPUIdleProcess();

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
long SysGetNextProcessInfo(RegisterContext* r) {
    pid_t* pidP = reinterpret_cast<pid_t*>(SC_ARG0(r));
    lemon_process_info_t* pInfo = reinterpret_cast<lemon_process_info_t*>(SC_ARG1(r));

    Process* cProcess = Scheduler::GetCurrentProcess();
    if (!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(lemon_process_info_t), cProcess->addressSpace)) {
        return -EFAULT;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), sizeof(pid_t), cProcess->addressSpace)) {
        return -EFAULT;
    }

    *pidP = Scheduler::GetNextProcessPID(*pidP);

    if (!(*pidP)) {
        return 1; // No more processes
    }

    FancyRefPtr<Process> reqProcess;
    if (!(reqProcess = Scheduler::FindProcessByPID(*pidP))) {
        return -EINVAL;
    }

    pInfo->pid = *pidP;

    pInfo->threadCount = reqProcess->Threads().get_length();

    pInfo->uid = reqProcess->uid;
    pInfo->gid = reqProcess->gid;

    pInfo->state = reqProcess->GetMainThread()->state;

    strcpy(pInfo->name, reqProcess->name);

    pInfo->runningTime = Timer::GetSystemUptime() - reqProcess->creationTime.tv_sec;
    pInfo->activeUs = reqProcess->activeTicks * 1000000 / Timer::GetFrequency();

    pInfo->usedMem = reqProcess->usedMemoryBlocks / 4;
    pInfo->isCPUIdle = reqProcess->IsCPUIdleProcess();

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
long SysReadLink(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), 0, proc->addressSpace)) {
        return -EFAULT; // Invalid path pointer
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), SC_ARG2(r), proc->addressSpace)) {
        return -EFAULT; // Invalid buffer
    }

    const char* path = reinterpret_cast<const char*>(SC_ARG0(r));
    char* buffer = reinterpret_cast<char*>(SC_ARG1(r));
    size_t bufferSize = SC_ARG2(r);

    FsNode* link = fs::ResolvePath(path);
    if (!link) {
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
long SysSpawnThread(RegisterContext* r) {
    auto tid = Process::Current()->CreateChildThread(SC_ARG0(r), SC_ARG1(r), USER_CS, USER_SS);

    return tid;
}

/////////////////////////////
/// \brief SysExitThread(retval) Exit a thread
///
/// \param retval - (void*) Return value pointer
///
/// \return Undefined, always succeeds
/////////////////////////////
long SysExitThread(RegisterContext* r) {
    Log::Warning("SysExitThread is unimplemented! Hanging!");

    GetCPULocal()->currentThread->blocker = nullptr;
    releaseLock(&GetCPULocal()->currentThread->lock);

    if(GetCPULocal()->currentThread->state == ThreadStateRunning) {
        GetCPULocal()->currentThread->state = ThreadStateBlocked;
    }

    GetCPULocal()->currentThread->state = ThreadStateZombie;
    return 0;
}

/////////////////////////////
/// \brief SysFutexWake(futex) Wake a thread waiting on a futex
///
/// \param futex - (int*) Futex pointer
///
/// \return 0 on success, error code on failure
/////////////////////////////
long SysFutexWake(RegisterContext* r) {
    int* futex = reinterpret_cast<int*>(SC_ARG0(r));

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), sizeof(int), Scheduler::GetCurrentProcess()->addressSpace)) {
        return EFAULT;
    }

    Process* currentProcess = Scheduler::GetCurrentProcess();

    List<FutexThreadBlocker*>* blocked = nullptr;
    if (!currentProcess->futexWaitQueue.get(reinterpret_cast<uintptr_t>(futex), blocked) || !blocked->get_length()) {
        return 0;
    }

    auto front = blocked->remove_at(0);

    if (front) {
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
long SysFutexWait(RegisterContext* r) {
    int* futex = reinterpret_cast<int*>(SC_ARG0(r));

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), sizeof(int), Scheduler::GetCurrentProcess()->addressSpace)) {
        return EFAULT;
    }

    int expected = static_cast<int>(SC_ARG1(r));

    if (*futex != expected) {
        return 0;
    }

    Process* currentProcess = Scheduler::GetCurrentProcess();
    Thread* currentThread = Scheduler::GetCurrentThread();

    List<FutexThreadBlocker*>* blocked;
    if (!currentProcess->futexWaitQueue.get(reinterpret_cast<uintptr_t>(futex), blocked)) {
        blocked = new List<FutexThreadBlocker*>();

        currentProcess->futexWaitQueue.insert(reinterpret_cast<uintptr_t>(futex), blocked);
    }

    FutexThreadBlocker blocker;
    blocked->add_back(&blocker);

    if (currentThread->Block(&blocker)) {
        blocked->remove(&blocker);

        return -EINTR; // We were interrupted
    }

    return 0;
}

/////////////////////////////
/// \brief SysDup(fd, flags, newfd) Duplicate a handle
///
/// \param fd (int) handle to duplicate
///
/// \return new handle (int) on success, negative error code on failure
/////////////////////////////
long SysDup(RegisterContext* r) {
    int fd = static_cast<int>(SC_ARG0(r));
    int requestedFd = static_cast<int>(SC_ARG2(r));

    if (fd == requestedFd) {
        return -EINVAL;
    }

    Process* currentProcess = Scheduler::GetCurrentProcess();
    long flags = SC_ARG1(r);

    Handle handle = currentProcess->GetHandle(fd);
    if (!handle) {
        Log::Debug(debugLevelSyscalls, DebugLevelNormal, "SysDup: Invalid handle %d", fd);
        return -EBADF;
    }

    if (flags & O_CLOEXEC) {
        handle.closeOnExec = true;
    }

    if (requestedFd >= 0) {
        if (static_cast<unsigned>(requestedFd) >= currentProcess->HandleCount()) {
            Log::Error("SysDup: We do not support (yet) requesting unallocated fds."); // TODO: Requested file
                                                                                       // descriptors out of range
            return -ENOSYS;
        }

        currentProcess->DestroyHandle(requestedFd); // If it exists destroy existing fd

        if (currentProcess->ReplaceHandle(requestedFd, std::move(handle))) {
            return -EBADF;
        }
        return requestedFd;
    } else {
        return currentProcess->AllocateHandle(std::move(handle.ko), handle.closeOnExec);
    }
}

/////////////////////////////
/// \brief SysGetFileStatusFlags(fd) Get a file handle's mode/status flags
///
/// \param fd (int) file descriptor
///
/// \return flags on success, negative error code on failure
/////////////////////////////
long SysGetFileStatusFlags(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(currentProcess->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
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
long SysSetFileStatusFlags(RegisterContext* r) {
    int nFlags = static_cast<int>(SC_ARG1(r));

    Process* currentProcess = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(currentProcess->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
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
long SysSelect(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    int nfds = static_cast<int>(SC_ARG0(r));
    fd_set_t* readFdsMask = reinterpret_cast<fd_set_t*>(SC_ARG1(r));
    fd_set_t* writeFdsMask = reinterpret_cast<fd_set_t*>(SC_ARG2(r));
    fd_set_t* exceptFdsMask = reinterpret_cast<fd_set_t*>(SC_ARG3(r));
    timespec_t* timeout = reinterpret_cast<timespec_t*>(SC_ARG4(r));

    if (!((!readFdsMask || Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(fd_set_t), currentProcess->addressSpace)) &&
          (!writeFdsMask || Memory::CheckUsermodePointer(SC_ARG2(r), sizeof(fd_set_t), currentProcess->addressSpace)) &&
          (!exceptFdsMask ||
           Memory::CheckUsermodePointer(SC_ARG3(r), sizeof(fd_set_t), currentProcess->addressSpace)) &&
          Memory::CheckUsermodePointer(SC_ARG4(r), sizeof(timespec_t), currentProcess->addressSpace))) {
        return -EFAULT; // Only return EFAULT if read/write/exceptfds is not null
    }

    List<Pair<FancyRefPtr<UNIXOpenFile>, int>> readfds;
    List<Pair<FancyRefPtr<UNIXOpenFile>, int>> writefds;
    List<Pair<FancyRefPtr<UNIXOpenFile>, int>> exceptfds;

    for (int i = 0; i < 128 && i * 8 < nfds; i++) {
        char read = 0, write = 0, except = 0;
        if (readFdsMask) {
            read = readFdsMask->fds_bits[i];
            readFdsMask->fds_bits[i] = 0; // With select the fds are cleared to 0 unless they are ready
        }

        if (writeFdsMask) {
            write = writeFdsMask->fds_bits[i];
            writeFdsMask->fds_bits[i] = 0;
        }

        if (exceptFdsMask) {
            except = exceptFdsMask->fds_bits[i];
            exceptFdsMask->fds_bits[i] = 0;
        }

        for (int j = 0; j < 8 && (i * 8 + j) < nfds; j++) {
            if ((read >> j) & 0x1) {
                FancyRefPtr<UNIXOpenFile> h = SC_TRY_OR_ERROR(currentProcess->GetHandleAs<UNIXOpenFile>(i * 8 + j));
                if (!h) {
                    return -EBADF;
                }

                readfds.add_back(Pair<FancyRefPtr<UNIXOpenFile>, int>(std::move(h), i * 8 + j));
            }

            if ((write >> j) & 0x1) {
                FancyRefPtr<UNIXOpenFile> h = SC_TRY_OR_ERROR(currentProcess->GetHandleAs<UNIXOpenFile>(i * 8 + j));
                if (!h) {
                    return -EBADF;
                }

                writefds.add_back(Pair<FancyRefPtr<UNIXOpenFile>, int>(std::move(h), i * 8 + j));
            }

            if ((except >> j) & 0x1) {
                FancyRefPtr<UNIXOpenFile> h = SC_TRY_OR_ERROR(currentProcess->GetHandleAs<UNIXOpenFile>(i * 8 + j));
                if (!h) {
                    return -EBADF;
                }

                exceptfds.add_back(Pair<FancyRefPtr<UNIXOpenFile>, int>(std::move(h), i * 8 + j));
            }
        }
    }

    if (exceptfds.get_length()) {
        // Log::Warning("SysSelect: ExceptFds ignored!");
    }

    int evCount = 0;

    FilesystemWatcher fsWatcher;

    for (auto& handle : readfds) {
        if (handle.item1->node->CanRead()) {
            FD_SET(handle.item2, readFdsMask);
            evCount++;
        } else if (!evCount) {
            fsWatcher.WatchNode(handle.item1->node, POLLIN);
        }
    }

    for (auto& handle : writefds) {
        if (handle.item1->node->CanWrite()) {
            FD_SET(handle.item2, writeFdsMask);
            evCount++;
        } else if (!evCount) {
            fsWatcher.WatchNode(handle.item1->node, POLLOUT);
        }
    }

    if (evCount) {
        return evCount;
    }

    long timeoutUs = 0;
    if (timeout) {
        timeoutUs = (timeout->tv_sec * 1000000) + (timeout->tv_nsec / 1000);
    }

    if (timeoutUs) {
        if (fsWatcher.WaitTimeout(timeoutUs)) {
            return -EINTR;
        }
    } else {
        if (fsWatcher.Wait()) {
            return -EINTR;
        }
    }

    for (auto& handle : readfds) {
        if (handle.item1->node->CanRead()) {
            FD_SET(handle.item2, readFdsMask);
            evCount++;
        }
    }

    for (auto& handle : writefds) {
        if (handle.item1->node->CanWrite()) {
            FD_SET(handle.item2, writeFdsMask);
            evCount++;
        }
    }

    // for(UNIXOpenFile* handle : exceptfds);

    return evCount;
}

/////////////////////////////
/// \brief SysCreateService (name) - Create a service
///
/// Create a new service. Services are essentially named containers for interfaces, allowing one program to implement
/// multiple interfaces under a service.
///
/// \param name (const char*) Service name to be used, must be unique
///
/// \return Handle ID on success, error code on failure
/////////////////////////////
long SysCreateService(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    size_t nameLength;
    if (strlenSafe(reinterpret_cast<const char*>(SC_ARG0(r)), nameLength, currentProcess->addressSpace)) {
        return -EFAULT;
    }

    char name[nameLength + 1];
    strncpy(name, reinterpret_cast<const char*>(SC_ARG0(r)), nameLength);
    name[nameLength] = 0;

    for (auto& svc : ServiceFS::Instance()->services) {
        if (strncmp(svc->GetName(), name, nameLength) == 0) {
            Log::Warning("SysCreateService: Service '%s' already exists!", svc->GetName());
            return -EEXIST;
        }
    }

    FancyRefPtr<Service> svc = ServiceFS::Instance()->CreateService(name);
    return currentProcess->AllocateHandle(static_pointer_cast<KernelObject, Service>(svc));
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
long SysCreateInterface(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    Handle svcHandle;
    if (!(svcHandle = currentProcess->GetHandle(SC_ARG0(r)))) {
        Log::Warning("SysCreateInterface: Invalid handle ID %d", SC_ARG0(r));
        return -EBADF;
    }

    if (!svcHandle.ko->IsType(Service::TypeID())) {
        Log::Warning("SysCreateInterface: Invalid handle type (ID %d)", SC_ARG0(r));
        return -EINVAL;
    }

    size_t nameLength;
    if (strlenSafe(reinterpret_cast<const char*>(SC_ARG1(r)), nameLength, currentProcess->addressSpace)) {
        return -EFAULT;
    }

    char name[nameLength + 1];
    strncpy(name, reinterpret_cast<const char*>(SC_ARG1(r)), nameLength);
    name[nameLength] = 0;

    Service* svc = reinterpret_cast<Service*>(svcHandle.ko.get());

    FancyRefPtr<MessageInterface> interface;
    long ret = svc->CreateInterface(interface, name, SC_ARG2(r));

    if (ret) {
        return ret;
    }

    return currentProcess->AllocateHandle(static_pointer_cast<KernelObject, MessageInterface>(interface));
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
long SysInterfaceAccept(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    Handle ifHandle;
    if (!(ifHandle = currentProcess->GetHandle(SC_ARG0(r)))) {
        Log::Warning("SysInterfaceAccept: Invalid handle ID %d", SC_ARG0(r));
        return -EBADF;
    }

    if (!ifHandle.ko->IsType(MessageInterface::TypeID())) {
        Log::Warning("SysInterfaceAccept: Invalid handle type (ID %d)", SC_ARG0(r));
        return -EINVAL;
    }

    MessageInterface* interface = reinterpret_cast<MessageInterface*>(ifHandle.ko.get());
    FancyRefPtr<MessageEndpoint> endp;
    if (long ret = interface->Accept(endp); ret <= 0) {
        return ret;
    }

    return currentProcess->AllocateHandle(static_pointer_cast<KernelObject, MessageEndpoint>(endp));
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
long SysInterfaceConnect(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    size_t sz = 0;
    if (strlenSafe(reinterpret_cast<const char*>(SC_ARG0(r)), sz, currentProcess->addressSpace)) {
        return -EFAULT;
    }

    char path[sz + 1];
    strncpy(path, reinterpret_cast<const char*>(SC_ARG0(r)), sz);
    path[sz] = 0;

    if (!strchr(path, '/')) { // Interface name given by '/' separator
        Log::Warning("SysInterfaceConnect: No interface name given!");
        return -EINVAL;
    }

    FancyRefPtr<MessageInterface> interface;
    {
        FancyRefPtr<Service> svc;
        if (ServiceFS::Instance()->ResolveServiceName(svc, path)) {
            Log::Warning("SysInterfaceConnect: No such service '%s'!", path);
            return -ENOENT; // No such service
        }

        if (svc->ResolveInterface(interface, strchr(path, '/') + 1)) {
            Log::Warning("SysInterfaceConnect: No such interface '%s'!", path);
            return -ENOENT; // No such interface
        }
    }

    FancyRefPtr<MessageEndpoint> endp = interface->Connect();
    if (!endp.get()) {
        Log::Warning("SysInterfaceConnect: Failed to connect!");
        return -EINVAL; // Some error connecting, interface destroyed?
    }

    return currentProcess->AllocateHandle(static_pointer_cast<KernelObject>(endp));
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
long SysEndpointQueue(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    Handle endpHandle;
    if (!(endpHandle = currentProcess->GetHandle(SC_ARG0(r)))) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
            Log::Warning("(%s): SysEndpointQueue: Invalid handle ID %d", currentProcess->name, SC_ARG0(r));
            Log::Info("%x", r->rip);
            UserPrintStackTrace(r->rbp, Scheduler::GetCurrentProcess()->addressSpace);
        });
        return -EBADF;
    }

    if (!endpHandle.ko->IsType(MessageEndpoint::TypeID())) {
        Log::Warning("SysEndpointQueue: Invalid handle type (ID %d)", SC_ARG0(r));
        return -EINVAL;
    }

    size_t size = SC_ARG2(r);
    if (size && !Memory::CheckUsermodePointer(SC_ARG3(r), size, currentProcess->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
            Log::Warning("(%s): SysEndpointQueue: Invalid data buffer %x", currentProcess->name, SC_ARG3(r));
            Log::Info("%x", r->rip);
            UserPrintStackTrace(r->rbp, Scheduler::GetCurrentProcess()->addressSpace);
        });
        return -EFAULT; // Data greater than 8 and invalid pointer
    }

    MessageEndpoint* endpoint = reinterpret_cast<MessageEndpoint*>(endpHandle.ko.get());

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
long SysEndpointDequeue(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    Handle endpHandle;
    if (!(endpHandle = currentProcess->GetHandle(SC_ARG0(r)))) {
        Log::Warning("(%s): SysEndpointDequeue: Invalid handle ID %d", currentProcess->name, SC_ARG0(r));
        return -EINVAL;
    }

    if (!endpHandle.ko->IsType(MessageEndpoint::TypeID())) {
        Log::Warning("SysEndpointDequeue: Invalid handle type (ID %d)", SC_ARG0(r));
        return -EINVAL;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(uint64_t), currentProcess->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
            Log::Warning("(%s): SysEndpointDequeue: Invalid ID pointer %x", currentProcess->name, SC_ARG3(r));
            Log::Info("%x", r->rip);
            UserPrintStackTrace(r->rbp, Scheduler::GetCurrentProcess()->addressSpace);
        });
        return -EFAULT;
    }

    if (!Memory::CheckUsermodePointer(SC_ARG2(r), sizeof(uint16_t), currentProcess->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
            Log::Warning("(%s): SysEndpointDequeue: Invalid size pointer %x", currentProcess->name, SC_ARG3(r));
            Log::Info("%x", r->rip);
            UserPrintStackTrace(r->rbp, Scheduler::GetCurrentProcess()->addressSpace);
        });
        return -EFAULT;
    }

    MessageEndpoint* endpoint = reinterpret_cast<MessageEndpoint*>(endpHandle.ko.get());

    if (!Memory::CheckUsermodePointer(SC_ARG3(r), endpoint->GetMaxMessageSize(), currentProcess->addressSpace)) {
        IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, {
            Log::Warning("(%s): SysEndpointDequeue: Invalid data buffer %x", currentProcess->name, SC_ARG3(r));
            Log::Info("%x", r->rip);
            UserPrintStackTrace(r->rbp, Scheduler::GetCurrentProcess()->addressSpace);
        });
        return -EFAULT;
    }

    return endpoint->Read(reinterpret_cast<uint64_t*>(SC_ARG1(r)), reinterpret_cast<uint16_t*>(SC_ARG2(r)),
                          reinterpret_cast<uint8_t*>(SC_ARG3(r)));
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
long SysEndpointCall(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    Handle endpHandle;
    if (!(endpHandle = currentProcess->GetHandle(SC_ARG0(r)))) {
        Log::Warning("(%s): SysEndpointCall: Invalid handle ID %d", currentProcess->name, SC_ARG0(r));
        return -EINVAL;
    }

    if (!endpHandle.ko->IsType(MessageEndpoint::TypeID())) {
        Log::Warning("SysEndpointCall: Invalid handle type (ID %d)", SC_ARG0(r));
        return -EINVAL;
    }

    uint16_t* size = reinterpret_cast<uint16_t*>(SC_ARG5(r));
    if (*size && !Memory::CheckUsermodePointer(SC_ARG2(r), *size, currentProcess->addressSpace)) {
        return -EFAULT; // Invalid data pointer
    }

    MessageEndpoint* endpoint = reinterpret_cast<MessageEndpoint*>(endpHandle.ko.get());

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
long SysEndpointInfo(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    MessageEndpointInfo* info = reinterpret_cast<MessageEndpointInfo*>(SC_ARG1(r));

    if (!Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(MessageEndpointInfo), currentProcess->addressSpace)) {
        return -EFAULT; // Data greater than 8 and invalid pointer
    }

    Handle endpHandle;
    if (!(endpHandle = currentProcess->GetHandle(SC_ARG0(r)))) {
        Log::Warning("(%s): SysEndpointInfo: Invalid handle ID %d", currentProcess->name, SC_ARG0(r));
        return -EINVAL;
    }

    if (!endpHandle.ko->IsType(MessageEndpoint::TypeID())) {
        Log::Warning("SysEndpointInfo: Invalid handle type (ID %d)", SC_ARG0(r));
        return -EINVAL;
    }

    MessageEndpoint* endpoint = reinterpret_cast<MessageEndpoint*>(endpHandle.ko.get());

    *info = {.msgSize = endpoint->GetMaxMessageSize()};
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
long SysKernelObjectWaitOne(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    Handle handle;
    if (!(handle = currentProcess->GetHandle(SC_ARG0(r)))) {
        Log::Warning("SysKernelObjectWaitOne: Invalid handle ID %d", SC_ARG0(r));
        return -EINVAL;
    }

    long timeout = SC_ARG1(r);

    KernelObjectWatcher watcher;

    watcher.WatchObject(handle.ko, 0);

    if (timeout > 0) {
        if (watcher.WaitTimeout(timeout)) {
            return -EINTR;
        }
    } else {
        if (watcher.Wait()) {
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
long SysKernelObjectWait(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();
    unsigned count = SC_ARG1(r);
    long timeout = SC_ARG2(r);

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), count * sizeof(handle_id_t), currentProcess->addressSpace)) {
        return -EFAULT;
    }

    Handle handles[count];
    UserBuffer<handle_id_t> handleIDs(SC_ARG0(r));

    KernelObjectWatcher watcher;
    for (unsigned i = 0; i < count; i++) {
        handle_id_t id;
        if(handleIDs.GetValue(i, id)) {
            return -EFAULT;
        }

        if (!(handles[i] = currentProcess->GetHandle(id))) {
            IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal,
                     { Log::Warning("SysKernelObjectWait: Invalid handle ID %d", SC_ARG0(r)); });
            return -EINVAL;
        }

        watcher.WatchObject(handles[i].ko, 0);
    }

    if (timeout > 0) {
        if (watcher.WaitTimeout(timeout)) {
            return -EINTR;
        }
    } else {
        if (watcher.Wait()) {
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
long SysKernelObjectDestroy(RegisterContext* r) {
    Process* currentProcess = Scheduler::GetCurrentProcess();

    Handle h;
    if (!(h = currentProcess->GetHandle(SC_ARG0(r)))) {
        Log::Warning("SysKernelObjectDestroy: Invalid handle ID %d", SC_ARG0(r));

        IF_DEBUG(debugLevelSyscalls >= DebugLevelVerbose, {
            Log::Info("Process %s (PID: %d), Stack Trace:", currentProcess->name, currentProcess->PID());
            Log::Info("%x", r->rip);
            UserPrintStackTrace(r->rbp, currentProcess->addressSpace);
        });
        return -EBADF;
    }

    Log::Warning("SysKernelObjectDestroy: destrying ko %d (type %d)", SC_ARG0(r), h.ko->InstanceTypeID());

    Log::Info("Process %s (PID: %d), Stack Trace:", currentProcess->name, currentProcess->PID());
    Log::Info("%x", r->rip);
    UserPrintStackTrace(r->rbp, currentProcess->addressSpace);

    currentProcess->DestroyHandle(SC_ARG0(r));
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
long SysSetSocketOptions(RegisterContext* r) {
    int fd = SC_ARG0(r);
    int level = SC_ARG1(r);
    int opt = SC_ARG2(r);
    const void* optVal = reinterpret_cast<void*>(SC_ARG3(r));
    socklen_t optLen = SC_ARG4(r);

    Process* currentProcess = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(currentProcess->GetHandleAs<UNIXOpenFile>(fd));
    if (!handle) {
        return -EBADF;
    }

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        return -ENOTSOCK; //  Not a socket
    }

    if (optLen && !Memory::CheckUsermodePointer(SC_ARG3(r), optLen, currentProcess->addressSpace)) {
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
long SysGetSocketOptions(RegisterContext* r) {
    int fd = SC_ARG0(r);
    int level = SC_ARG1(r);
    int opt = SC_ARG2(r);
    void* optVal = reinterpret_cast<void*>(SC_ARG3(r));
    socklen_t* optLen = reinterpret_cast<socklen_t*>(SC_ARG4(r));

    Process* currentProcess = Scheduler::GetCurrentProcess();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(currentProcess->GetHandleAs<UNIXOpenFile>(fd));
    if (!handle) {
        return -EBADF;
    }

    if ((handle->node->flags & FS_NODE_TYPE) != FS_NODE_SOCKET) {
        return -ENOTSOCK; //  Not a socket
    }

    if (!Memory::CheckUsermodePointer(SC_ARG4(r), sizeof(socklen_t), currentProcess->addressSpace)) {
        return -EFAULT;
    }

    if (*optLen && !Memory::CheckUsermodePointer(SC_ARG3(r), *optLen, currentProcess->addressSpace)) {
        return -EFAULT;
    }

    Socket* sock = reinterpret_cast<Socket*>(handle->node);
    return sock->GetSocketOptions(level, opt, optVal, optLen);
}

long SysDeviceManagement(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    int64_t request = SC_ARG0(r);

    if (request == DeviceManager::RequestDeviceManagerGetRootDeviceCount) {
        return DeviceManager::DeviceCount();
    } else if (request == DeviceManager::RequestDeviceManagerEnumerateRootDevices) {
        int64_t offset = SC_ARG1(r);
        int64_t requestedDeviceCount = SC_ARG2(r);
        int64_t* idList = reinterpret_cast<int64_t*>(SC_ARG3(r));

        if (!Memory::CheckUsermodePointer(SC_ARG3(r), sizeof(int64_t) * requestedDeviceCount, process->addressSpace)) {
            return -EFAULT;
        }

        return DeviceManager::EnumerateDevices(offset, requestedDeviceCount, idList);
    }

    switch (request) {
    case DeviceManager::RequestDeviceResolveFromPath: {
        const char* path = reinterpret_cast<const char*>(SC_ARG1(r));

        size_t pathLen;
        if (strlenSafe(path, pathLen, process->addressSpace)) {
            return -EFAULT;
        }

        Device* dev = DeviceManager::ResolveDevice(path);
        if (!dev) {
            return -ENOENT; // No such device
        }

        return dev->ID();
    }
    case DeviceManager::RequestDeviceGetName: {
        int64_t deviceID = SC_ARG1(r);
        char* name = reinterpret_cast<char*>(SC_ARG2(r));
        size_t nameBufferSize = SC_ARG3(r);

        if (!Memory::CheckUsermodePointer(SC_ARG2(r), nameBufferSize, process->addressSpace)) {
            return -EFAULT;
        }

        Device* dev = DeviceManager::DeviceFromID(deviceID);
        if (!dev) {
            return -ENOENT;
        }

        strncpy(name, dev->DeviceName().c_str(), nameBufferSize);
        return 0;
    }
    case DeviceManager::RequestDeviceGetInstanceName: {
        int64_t deviceID = SC_ARG1(r);
        char* name = reinterpret_cast<char*>(SC_ARG2(r));
        size_t nameBufferSize = SC_ARG3(r);

        if (!Memory::CheckUsermodePointer(SC_ARG2(r), nameBufferSize, process->addressSpace)) {
            return -EFAULT;
        }

        Device* dev = DeviceManager::DeviceFromID(deviceID);
        if (!dev) {
            return -ENOENT;
        }

        strncpy(name, dev->InstanceName().c_str(), nameBufferSize);
        return 0;
    }
    case DeviceManager::RequestDeviceGetPCIInformation:
        return -ENOSYS;
    case DeviceManager::RequestDeviceIOControl:
        return -ENOSYS;
    case DeviceManager::RequestDeviceGetType: {
        long deviceID = SC_ARG1(r);

        if (!Memory::CheckUsermodePointer(SC_ARG2(r), sizeof(long), process->addressSpace)) {
            return -EFAULT;
        }

        Device* dev = DeviceManager::DeviceFromID(deviceID);
        if (!dev) {
            return -ENOENT;
        }

        return dev->Type();
    }
    case DeviceManager::RequestDeviceGetChildCount:
        return -ENOSYS;
    case DeviceManager::RequestDeviceEnumerateChildren:
        return -ENOSYS;
    default:
        return -EINVAL;
    }
}

long SysInterruptThread(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    long tid = SC_ARG0(r);

    auto th = process->GetThreadFromTID(tid);
    if (!th.get()) {
        return -ESRCH; // Thread has already been killed
    }

    if (th->blocker) {
        th->blocker->Interrupt(); // Stop the thread from blocking
        th->Unblock();
    }

    return 0;
}

/////////////////////////////
/// \brief SysLoadKernelModule(path)
///
/// Load a kernel module
///
/// \param path Filepath of module to load

/// \return 0 on success negative error code on failure
/////////////////////////////
long SysLoadKernelModule(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    if (process->euid != 0) {
        return -EPERM; // Must be root
    }

    size_t filePathLength;
    long filePathInvalid = strlenSafe(reinterpret_cast<char*>(SC_ARG0(r)), filePathLength, process->addressSpace);
    if (filePathInvalid) {
        Log::Warning("SysLoadKernelModule: Reached unallocated memory reading file path");
        return -EFAULT;
    }

    char filepath[filePathLength + 1];
    strncpy(filepath, (char*)SC_ARG0(r), filePathLength);

    ModuleLoadStatus status = ModuleManager::LoadModule(filepath);
    if (status.status != ModuleLoadStatus::ModuleOK) {
        return -EINVAL;
    } else {
        return 0;
    }
}

/////////////////////////////
/// \brief SysUnloadKernelModule(name)
///
/// Unload a kernel module
///
/// \param name Name of kernel module to unload

/// \return 0 on success negative error code on failure
/////////////////////////////
long SysUnloadKernelModule(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    if (process->euid != 0) {
        return -EPERM; // Must be root
    }

    size_t nameLength;
    long nameInvalid = strlenSafe(reinterpret_cast<char*>(SC_ARG0(r)), nameLength, process->addressSpace);
    if (nameInvalid) {
        Log::Warning("SysUnloadKernelModule: Reached unallocated memory reading file path");
        return -EFAULT;
    }

    char name[nameLength + 1];
    strncpy(name, (char*)SC_ARG0(r), nameLength);

    return ModuleManager::UnloadModule(name);
}

/////////////////////////////
/// \brief SysFork()
///
///	Clone's a process's address space (with copy-on-write), file descriptors and register state
///
/// \return Child PID to the calling process
/// \return 0 to the newly forked child
/////////////////////////////
long SysFork(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();
    Thread* currentThread = Scheduler::GetCurrentThread();

    FancyRefPtr<Process> newProcess = process->Fork();
    FancyRefPtr<Thread> thread = newProcess->GetMainThread();
    void* threadKStack = thread->kernelStack; // Save the allocated kernel stack

    *thread = *currentThread;
    thread->kernelStack = threadKStack;
    thread->state = ThreadStateRunning;
    thread->parent = newProcess.get();
    thread->registers = *r;

    thread->lock = 0;
    thread->stateLock = 0;

    thread->tid = 1;

    thread->blocker = nullptr;
    thread->blockTimedOut = false;

    thread->registers.rax = 0; // To the child we return 0

    newProcess->Start();
    return newProcess->PID(); // Return PID to parent process
}

/////////////////////////////
/// \brief SysGetGID()
///
/// \return Current process's Group ID
/////////////////////////////
long SysGetGID(RegisterContext* r) { return Scheduler::GetCurrentProcess()->gid; }

/////////////////////////////
/// \brief SysGetEGID()
///
/// \return Current process's effective Group ID
/////////////////////////////
long SysGetEGID(RegisterContext* r) { return Scheduler::GetCurrentProcess()->egid; }

/////////////////////////////
/// \brief SysGetEGID()
///
/// \return PID of parent process, if there is no parent then -1
/////////////////////////////
long SysGetPPID(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();
    if (process->Parent()) {
        return process->Parent()->PID();
    } else {
        return -1;
    }
}

/////////////////////////////
/// \brief SysPipe(fds, flags)
///
/// \param fds (int[2]) read end fd is placed in fds[0], write end in fds[1]
/// \param flags Pipe flags can be any of O_NONBLOCK and O_CLOEXEC
///
/// \return Negative error code on error, otherwise 0
/////////////////////////////
long SysPipe(RegisterContext* r) {
    int* fds = reinterpret_cast<int*>(SC_ARG0(r));
    int flags = SC_ARG1(r);

    if (flags & ~(O_CLOEXEC | O_NONBLOCK)) {
        Log::Debug(debugLevelSyscalls, DebugLevelNormal, "SysPipe: Invalid flags %d", flags);
        return -EINVAL;
    }

    Process* process = Scheduler::GetCurrentProcess();

    UNIXPipe* read;
    UNIXPipe* write;

    UNIXPipe::CreatePipe(read, write);

    UNIXOpenFile* readHandle = SC_TRY_OR_ERROR(fs::Open(read));
    UNIXOpenFile* writeHandle = SC_TRY_OR_ERROR(fs::Open(write));

    readHandle->mode = flags;
    writeHandle->mode = flags;

    fds[0] = process->AllocateHandle(readHandle);
    fds[1] = process->AllocateHandle(writeHandle);

    return 0;
}

/////////////////////////////
/// \brief SysGetEntropy(buf, length)
///
/// \param buf Buffer to be filled
/// \param length Size of buf, cannot be greater than 256
///
/// \return Negative error code on error, otherwise 0
///
/// \note Length cannot be greater than 256, if so -EIO is reutrned
/////////////////////////////
long SysGetEntropy(RegisterContext* r) {
    size_t length = SC_ARG1(r);
    if (length > 256) {
        return -EIO;
    }

    uint8_t* buffer = reinterpret_cast<uint8_t*>(SC_ARG0(r));
    if (!Memory::CheckUsermodePointer(SC_ARG0(r), length, Scheduler::GetCurrentProcess()->addressSpace)) {
        return -EFAULT; // buffer is invalid
    }

    while (length >= 8) {
        uint64_t value = Hash<uint64_t>(rand() % 65535 * (Timer::UsecondsSinceBoot() % 65535));

        *(reinterpret_cast<uint64_t*>(buffer)) = value;
        buffer += 8;
        length -= 8;
    }

    if (length > 0) {
        uint64_t value = Hash<uint64_t>(rand() % 65535 * (Timer::UsecondsSinceBoot() % 65535));
        memcpy(buffer, &value, length);
    }

    return 0;
}

/////////////////////////////
/// \brief SysSocketPair(int domain, int type, int protocol, int sv[2]);
///
/// \param domain Socket domain (muct be AF_UNIX/AF_LOCAL)
/// \param type Socket type
/// \param protocol Socket protocol
/// \param sv file descriptors for the created socket pair
///
/// \return Negative error code on failure, otherwise 0
/////////////////////////////
long SysSocketPair(RegisterContext* r) {
    Process* process = Scheduler::GetCurrentProcess();

    int domain = SC_ARG0(r);
    int type = SC_ARG1(r);
    int protocol = SC_ARG2(r);
    int* sv = reinterpret_cast<int*>(SC_ARG3(r));

    if (!Memory::CheckUsermodePointer(SC_ARG3(r), sizeof(int) * 2, process->addressSpace)) {
        Log::Warning("SysSocketPair: Invalid fd array!");
        return -EFAULT;
    }

    bool nonBlock = type & SOCK_NONBLOCK;
    type &= ~SOCK_NONBLOCK;

    if (domain != UnixDomain && domain != InternetProtocol) {
        Log::Warning("SysSocketPair: domain %d is not supported", domain);
        return -EAFNOSUPPORT;
    }
    if (type != StreamSocket && type != DatagramSocket) {
        Log::Warning("SysSocketPair: type %d is not supported", type);
        return -EPROTONOSUPPORT;
    }

    LocalSocket* s1 = new LocalSocket(type, protocol);
    LocalSocket* s2 = LocalSocket::CreatePairedSocket(s1);

    UNIXOpenFile* s1Handle = SC_TRY_OR_ERROR(fs::Open(s1));
    UNIXOpenFile* s2Handle = SC_TRY_OR_ERROR(fs::Open(s2));

    s1Handle->mode = nonBlock * O_NONBLOCK;
    s2Handle->mode = s1Handle->mode;

    sv[0] = process->AllocateHandle(s1Handle);
    sv[1] = process->AllocateHandle(s2Handle);

    assert(s1->IsConnected() && s2->IsConnected());
    return 0;
}

/////////////////////////////
/// \brief SysPeername(int sockfd, struct sockaddr* addr, socklen_t* addrlen) - get name of peer socket
///
/// Returns the address of the peer connected to the socket \a sockfd in \a addr
/// The \a addrlen argument should indicate the amount of space pointed to by \a addr.
/// On return it will contain the size of the name
/// The address will be truncated if the addr buffer is too small
///
/// \param sockfd File descriptor of socket
/// \param addr Address buffer
/// \param addrlen
///
/// \return Negative error code on failure, otherwise 0
/////////////////////////////
long SysPeername(RegisterContext* r) { return -ENOSYS; }

/////////////////////////////
/// \brief SysPeername(int sockfd, struct sockaddr* addr, socklen_t* addrlen) - get name of peer socket
///
/// Returns the address that the socket is bound to
/// The \a addrlen argument should indicate the amount of space pointed to by \a addr.
/// On return it will contain the size of the name
/// The address will be truncated if the addr buffer is too small
///
/// \param sockfd File descriptor of socket
/// \param addr Address buffer
/// \param addrlen
///
/// \return Negative error code on failure, otherwise 0
/////////////////////////////
long SysSockname(RegisterContext* r) { return -ENOSYS; }

long SysSignalAction(RegisterContext* r) {
    int signal = SC_ARG0(r); // Signal number
    switch (signal) {
    // SIGCANCEL is a special case
    // Can be used for pthread cancellation (we dont support this yet)
    case SIGCANCEL:
        Log::Debug(debugLevelSyscalls, DebugLevelNormal, "SysSignalAction: SIGCANCEL unsupported, returning ENOSYS");
        return -ENOSYS;
    // Supported and overridable signals
    case SIGINT:
    case SIGWINCH:
    case SIGABRT:
    case SIGALRM:
    case SIGCHLD:
    case SIGPIPE:
    case SIGUSR1:
    case SIGUSR2:
        break;
    // Cannot be overriden
    case SIGKILL:
    case SIGCONT:
    case SIGSTOP:
        Log::Debug(debugLevelSyscalls, DebugLevelNormal, "SysSignalAction: Signal %d cannot be overriden or ignored!",
                   signal);
        return -EINVAL; // Invalid signal for sigaction
    // Unsupported signal
    default:
        Log::Debug(debugLevelSyscalls, DebugLevelNormal, "SysSignalAction: Unsupported signal %d!", signal);
        return -EINVAL; // Invalid signal for sigaction
    }
    assert(signal < SIGNAL_MAX);

    const sigaction* sa = reinterpret_cast<sigaction*> SC_ARG1(r); // If non-null, new sigaction to set
    sigaction* oldSA = reinterpret_cast<sigaction*> SC_ARG2(r);    // If non-null, filled with old sigaction

    Process* proc = Scheduler::GetCurrentProcess();

    if (sa && !Scheduler::CheckUsermodePointer(sa)) {
        Log::Debug(debugLevelSyscalls, DebugLevelNormal, "SysSignalAction: Invalid sigaction pointer!");
        return -EFAULT;
    }

    if (oldSA) {
        if (!Scheduler::CheckUsermodePointer(oldSA)) {
            Log::Debug(debugLevelSyscalls, DebugLevelNormal, "SysSignalAction: Invalid old sigaction pointer!");
            return -EFAULT;
        }

        const SignalHandler& sigHandler = proc->signalHandlers[signal - 1];
        if (sigHandler.action == SignalHandler::HandlerAction::UsermodeHandler) {
            if (sigHandler.flags & SignalHandler::FlagSignalInfo) {
                *oldSA = {
                    .sa_handler = nullptr,
                    .sa_mask = sigHandler.mask,
                    .sa_flags = sigHandler.flags,
                    .sa_sigaction = reinterpret_cast<void (*)(int, siginfo_t*, void*)>(sigHandler.userHandler),
                };
            } else {
                *oldSA = {
                    .sa_handler = reinterpret_cast<void (*)(int)>(sigHandler.userHandler),
                    .sa_mask = sigHandler.mask,
                    .sa_flags = sigHandler.flags,
                    .sa_sigaction = nullptr,
                };
            }
        } else if (sigHandler.action == SignalHandler::HandlerAction::Default) {
            *oldSA = {
                .sa_handler = SIG_DFL,
                .sa_mask = sigHandler.mask,
                .sa_flags = sigHandler.flags,
                .sa_sigaction = nullptr,
            };
        } else if (sigHandler.action == SignalHandler::HandlerAction::Ignore) {
            *oldSA = {
                .sa_handler = SIG_IGN,
                .sa_mask = sigHandler.mask,
                .sa_flags = sigHandler.flags,
                .sa_sigaction = nullptr,
            };
        } else {
            Log::Error("Invalid signal action!");
            assert(!"Invalid signal action");
        }
    }

    if (!sa) {
        return 0; // sa not specified, nothing left to do
    }

    if (sa->sa_flags & (~SignalHandler::supportedFlags)) {
        Log::Error("SysSignalAction: (sig %d) sa_flags %x not supported!", signal, sa->sa_flags);
        return -EINVAL;
    }

    SignalHandler handler;
    if (sa->sa_handler == SIG_DFL) {
        handler.action = SignalHandler::HandlerAction::Default;
    } else if (sa->sa_handler == SIG_IGN) {
        handler.action = SignalHandler::HandlerAction::Ignore;
    } else {
        handler.action = SignalHandler::HandlerAction::UsermodeHandler;
        if (sa->sa_flags & SignalHandler::FlagSignalInfo) {
            handler.userHandler = (void*)sa->sa_sigaction;
        } else {
            handler.userHandler = (void*)sa->sa_handler;
        }
    }
    handler.mask = sa->sa_mask;

    proc->signalHandlers[signal - 1] = handler;
    return 0;
}

long SysProcMask(RegisterContext* r) {
    Thread* t = Scheduler::GetCurrentThread();

    (void)t;
    Log::Debug(debugLevelSyscalls, DebugLevelNormal, "SysProcMask is a stub!");

    return -ENOSYS;
}

long SysKill(RegisterContext* r) {
    pid_t pid = SC_ARG0(r);
    int signal = SC_ARG1(r);

    FancyRefPtr<Process> victim = Scheduler::FindProcessByPID(pid);
    if (!victim.get()) {
        Log::Debug(debugLevelSyscalls, DebugLevelNormal, "SysKill: Process with PID %d does not exist!", pid);
        return -ESRCH; // Process does not exist
    }

    victim->GetMainThread()->Signal(signal);
    return 0;
}

long SysSignalReturn(RegisterContext* r) {
    Thread* th = Scheduler::GetCurrentThread();
    uint64_t* threadStack = reinterpret_cast<uint64_t*>(r->rsp);

    threadStack++;                     // Discard signal handler address
    threadStack++;                     // Discard padding
    th->signalMask = *(threadStack++); // Get the old signal mask
    // Do not allow the thread to modify CS or SS
    memcpy(r, threadStack, offsetof(RegisterContext, cs));
    r->rsp = reinterpret_cast<RegisterContext*>(threadStack)->rsp;
    // Only allow the following to be changed:
    // Carry, parity, aux carry, zero, sign, direction, overflow
    r->rflags = reinterpret_cast<RegisterContext*>(threadStack)->rflags & 0xcd5;

    // TODO: Signal FPU state save and restore

    return r->rax; // Ensure we keep the RAX value from before
}

long SysAlarm(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();

    proc->SetAlarm(SC_ARG0(r));

    return 0;
}

long SysGetResourceLimit(RegisterContext* r) {
    Process* proc = Scheduler::GetCurrentProcess();
    int resource = SC_ARG0(r);
    // struct rlimit* = SC_ARG1(r);

    (void)proc;
    (void)resource;

    return -ENOSYS;
}

long SysEpollCreate(RegisterContext* r) {
    Process* proc = Process::Current();
    int flags = SC_ARG0(r);

    if (flags & (~EPOLL_CLOEXEC)) {
        return -EINVAL;
    }

    fs::EPoll* ep = new fs::EPoll();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(ep->Open(0));
    handle->node = ep;
    handle->mode = 0;
    if (flags & EPOLL_CLOEXEC) {
        handle->mode |= O_CLOEXEC;
    }

    int fd = proc->AllocateHandle(handle);
    return fd;
}

long SysEPollCtl(RegisterContext* r) {
    Process* proc = Process::Current();

    int epfd = SC_ARG0(r);
    int op = SC_ARG1(r);
    int fd = SC_ARG2(r);
    UserPointer<struct epoll_event> event = SC_ARG3(r);

    auto epHandle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(epfd));
    auto handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(fd));

    if (!(handle && epHandle)) {
        return -EBADF; // epfd or fd is invalid
    }

    if (handle->node->IsEPoll()) {
        Log::Warning("SysEPollCtl: Currently watching other epoll devices is not supported.");
    }

    if (epfd == fd) {
        Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "epfd == fd");
        return -EINVAL;
    } else if (!epHandle->node->IsEPoll()) {
        Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "Not an epoll device!");
        return -EINVAL; // Not an epoll device
    }

    fs::EPoll* epoll = (fs::EPoll*)epHandle->node;

    ScopedSpinLock<true> lockEp(epoll->epLock);

    if (op == EPOLL_CTL_ADD) {
        struct epoll_event e;
        TRY_GET_UMODE_VALUE(event, e);

        if (e.events & EPOLLEXCLUSIVE) {
            // Unsupported
            Log::Warning("SysEPollCtl: EPOLLEXCLUSIVE unsupported!");
        }

        if (e.events & EPOLLET) {
            Log::Warning("SysEPollCtl: EPOLLET unsupported!");
        }

        for (const auto& pair : epoll->fds) {
            if (pair.item1 == fd) {
                return -EEXIST; // Already watching the fd
            }
        }

        epoll->fds.add_back({fd, {e.events, e.data}});
    } else if (op == EPOLL_CTL_DEL) {
        for (auto it = epoll->fds.begin(); it != epoll->fds.end(); it++) {
            if (it->item1 == fd) {
                epoll->fds.remove(it);
                return 0;
            }
        }

        return -ENOENT;
    } else if (op == EPOLL_CTL_MOD) {
        struct epoll_event* current = nullptr;
        struct epoll_event e;
        TRY_GET_UMODE_VALUE(event, e);

        if (e.events & EPOLLEXCLUSIVE) {
            // Not that we support this flag yet,
            // however for future it can only be
            // enabled on EPOLL_CTL_ADD.
            return -EINVAL;
        }

        for (auto& pair : epoll->fds) {
            if (pair.item1 == fd) {
                current = &pair.item2;
                break;
            }
        }

        if (!current) {
            return -ENOENT; // fd not found
        }
    }

    return -EINVAL;
}

long SysEpollWait(RegisterContext* r) {
    Process* proc = Process::Current();
    Thread* thread = Thread::Current();

    int epfd = SC_ARG0(r);
    UserBuffer<struct epoll_event> events = SC_ARG1(r);
    int maxevents = SC_ARG2(r);
    long timeout = SC_ARG3(r) * 1000;

    auto epHandle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(epfd));
    if (!epHandle) {
        return -EBADF;
    } else if (!epHandle->node->IsEPoll()) {
        Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "Not an epoll device!");
        return -EINVAL; // Not an epoll device
    }

    if (maxevents <= 0) {
        Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "maxevents cannot be <= 0");
        return -EINVAL; // maxevents cannot be <= 0
    }

    fs::EPoll* epoll = (fs::EPoll*)epHandle->node;

    int64_t sigmask = SC_ARG4(r);
    int64_t oldsigmask = 0;
    if (sigmask) {
        oldsigmask = thread->signalMask;
        thread->signalMask = sigmask;
    }

    OnCleanup([sigmask, oldsigmask, thread]() {
        if (sigmask) {
            thread->signalMask = oldsigmask;
        }
    });

    ScopedSpinLock lockEp(epoll->epLock);

    Vector<int> removeFds; // Fds to unwatch
    auto getEvents = [removeFds](FancyRefPtr<UNIXOpenFile>& handle, uint32_t requested, int& evCount) -> uint32_t {
        uint32_t ev = 0;
        if (requested & EPOLLIN) {
            if (handle->node->CanRead()) {
                ev |= EPOLLIN;
            }
        }

        if (requested & EPOLLOUT) {
            if (handle->node->CanWrite()) {
                ev |= EPOLLOUT;
            }
        }

        if (handle->node->IsSocket()) {
            if (!((Socket*)handle->node)->IsConnected() && !((Socket*)handle->node)->IsListening()) {
                ev |= EPOLLHUP;
            }

            if (((Socket*)handle->node)->PendingConnections() && (requested & EPOLLIN)) {
                ev |= EPOLLIN;
            }
        }

        if (ev) {
            evCount++;
        }

        return ev;
    };

    auto epollToPollEvents = [](uint32_t ep) -> int {
        int evs = 0;
        if (ep & EPOLLIN) {
            evs |= POLLIN;
        }

        if (ep & EPOLLOUT) {
            evs |= POLLOUT;
        }

        if (ep & EPOLLHUP) {
            evs |= POLLHUP;
        }

        if (ep & EPOLLRDHUP) {
            evs |= POLLRDHUP;
        }

        if (ep & EPOLLERR) {
            evs |= POLLERR;
        }

        if (ep & EPOLLPRI) {
            evs |= POLLPRI;
        }

        return evs;
    };

    struct EPollFD {
        FancyRefPtr<UNIXOpenFile> handle;
        int fd;
        struct epoll_event ev;
    };

    Vector<EPollFD> files;
    int evCount = 0; // Amount of fds with events
    for (const auto& pair : epoll->fds) {
        auto result = proc->GetHandleAs<UNIXOpenFile>(pair.item1);
        if (result.HasError()) {
            continue; // Ignore closed fds
        }

        auto handle = std::move(result.Value());

        if (uint32_t ev = getEvents(handle, pair.item2.events, evCount); ev) {
            if (events.StoreValue(evCount - 1, {
                                                   .events = ev,
                                                   .data = pair.item2.data,
                                               })) {
                return -EFAULT;
            }
        }

        if(evCount > 0) {
            if(pair.item2.events & EPOLLONESHOT) {
                removeFds.add_back(pair.item1);
            }
        } else {
            files.add_back({handle, pair.item1, pair.item2}); // We only need the handle for later if events were not found
        }

        if (evCount >= maxevents) {
            goto done; // Reached max events/fds
        }
    }

    if (evCount > 0) {
        goto done;
    }

    if (!evCount && timeout) {
        FilesystemWatcher fsWatcher;
        for (auto& file : files) {
            fsWatcher.WatchNode(file.handle->node, epollToPollEvents(file.ev.events));
        }

        while (!evCount) {
            if (timeout > 0) {
                if (fsWatcher.WaitTimeout(timeout)) {
                    return -EINTR; // Interrupted
                } else if (timeout <= 0) {
                    return 0; // Timed out
                }
            } else if (fsWatcher.Wait()) {
                return -EINTR; // Interrupted
            }

            for (auto& handle : files) {
                if (uint32_t ev = getEvents(handle.handle, handle.ev.events, evCount); ev) {
                    if (events.StoreValue(evCount - 1, {
                                                           .events = ev,
                                                           .data = handle.ev.data,
                                                       })) {
                        return -EFAULT;
                    }
                }

                if(evCount > 0) {
                    if(handle.ev.events & EPOLLONESHOT) {
                        removeFds.add_back(handle.fd);
                    }
                }

                if (evCount >= maxevents) {
                    goto done; // Reached max events/fds
                }
            }
        }
    }

done:
    for(int fd : removeFds) {
        for(auto it = epoll->fds.begin(); it != epoll->fds.end(); it++) {
            if(it->item1 == fd) {
                epoll->fds.remove(it);
                break;
            }
        }
    }

    return evCount;
}

syscall_t syscalls[NUM_SYSCALLS]{
    SysDebug,
    SysExit, // 1
    SysExec,
    SysRead,
    SysWrite,
    SysOpen, // 5
    SysCloseHandle,
    SysSleep,
    SysCreate,
    SysLink,
    SysUnlink, // 10
    SysExecve,
    SysChdir,
    SysTime,
    SysMapFB,
    SysGetTID, // 15
    SysChmod,
    SysFStat,
    SysStat,
    SysLSeek,
    SysGetPID, // 20
    SysMount,
    SysMkdir,
    SysRmdir,
    SysRename,
    SysYield, // 25
    SysReadDirNext,
    SysRenameAt,
    SysSendMessage,
    SysReceiveMessage,
    SysUptime, // 30
    SysGetVideoMode,
    SysUName,
    SysReadDir,
    SysSetFsBase,
    SysMmap, // 35
    nullptr,
    SysGetCWD,
    SysWaitPID,
    SysNanoSleep,
    SysPRead, // 40
    SysPWrite,
    SysIoctl,
    SysInfo,
    SysMunmap,
    SysCreateSharedMemory, // 45
    SysMapSharedMemory,
    SysUnmapSharedMemory,
    SysDestroySharedMemory,
    SysSocket,
    SysBind, // 50
    SysListen,
    SysAccept,
    SysConnect,
    SysSend,
    SysSendTo, // 55
    SysReceive,
    SysReceiveFrom,
    SysGetUID,
    SysSetUID,
    SysPoll, // 60
    SysSendMsg,
    SysRecvMsg,
    SysGetEUID,
    SysSetEUID,
    SysGetProcessInfo, // 65
    SysGetNextProcessInfo,
    SysReadLink,
    SysSpawnThread,
    SysExitThread,
    SysFutexWake, // 70
    SysFutexWait,
    SysDup,
    SysGetFileStatusFlags,
    SysSetFileStatusFlags,
    SysSelect, // 75
    SysCreateService,
    SysCreateInterface,
    SysInterfaceAccept,
    SysInterfaceConnect,
    SysEndpointQueue, // 80
    SysEndpointDequeue,
    SysEndpointCall,
    SysEndpointInfo,
    SysKernelObjectWaitOne,
    SysKernelObjectWait, // 85
    SysKernelObjectDestroy,
    SysSetSocketOptions,
    SysGetSocketOptions,
    SysDeviceManagement,
    SysInterruptThread, // 90
    SysLoadKernelModule,
    SysUnloadKernelModule,
    SysFork,
    SysGetGID,
    SysGetEGID, // 95
    SysGetPPID,
    SysPipe,
    SysGetEntropy,
    SysSocketPair,
    SysPeername, // 100
    SysSockname,
    SysSignalAction,
    SysProcMask,
    SysKill,
    SysSignalReturn, // 105
    SysAlarm,
    SysGetResourceLimit,
    SysEpollCreate,
    SysEPollCtl,
    SysEpollWait,   // 110
};

void DumpLastSyscall(Thread* t) {
    RegisterContext& lastSyscall = t->lastSyscall;
    Log::Info("Last syscall:\nCall: %d, arg0: %i (%x), arg1: %i (%x), arg2: %i (%x), arg3: %i (%x), arg4: %i (%x), "
              "arg5: %i (%x)",
              lastSyscall.rax, SC_ARG0(&lastSyscall), SC_ARG0(&lastSyscall), SC_ARG1(&lastSyscall),
              SC_ARG1(&lastSyscall), SC_ARG2(&lastSyscall), SC_ARG2(&lastSyscall), SC_ARG3(&lastSyscall),
              SC_ARG3(&lastSyscall), SC_ARG4(&lastSyscall), SC_ARG4(&lastSyscall), SC_ARG5(&lastSyscall),
              SC_ARG5(&lastSyscall));
}

extern "C" void SyscallHandler(RegisterContext* regs) {
    if (__builtin_expect(regs->rax >= NUM_SYSCALLS || !syscalls[regs->rax],
                         0)) { // If syscall is non-existant then return
        regs->rax = -ENOSYS;
        return;
    }

    asm("sti"); // By reenabling interrupts, a thread in a syscall can be preempted

    Thread* thread = GetCPULocal()->currentThread;
    if (__builtin_expect(thread->state == ThreadStateZombie, 0))
        for (;;)
            ;

#ifdef KERNEL_DEBUG
    if (debugLevelSyscalls >= DebugLevelNormal) {
        thread->lastSyscall = *regs;
    }
#endif
    if (__builtin_expect(acquireTestLock(&thread->lock), 0)) {
        Log::Info("Thread hanging!");
        for (;;)
            ;
    }

    regs->rax = syscalls[regs->rax](regs); // Call syscall

    IF_DEBUG((debugLevelSyscalls >= DebugLevelVerbose), {
        if(regs->rax < 0)
            Log::Info("Syscall %d exiting with value %d", thread->lastSyscall.rax, regs->rax);
    });

    if (__builtin_expect(thread->pendingSignals & (~thread->EffectiveSignalMask()), 0)) {
        thread->HandlePendingSignal(regs);
    }
    releaseLock(&thread->lock);
}