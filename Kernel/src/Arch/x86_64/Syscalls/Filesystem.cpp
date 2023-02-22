#include <Scheduler.h>
#include <Syscalls.h>

#include <Fs/Pipe.h>
#include <Net/Socket.h>

#include <StackTrace.h>
#include <UserPointer.h>

#include <asm/ioctls.h>
#define FIONCLEX 0x5450
#define FIOCLEX 0x5451

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
            String basename = fs::BaseName(filepath);

            if (!parent) {
                Log::Warning("SysOpen: Could not resolve parent directory of new file %s", basename.c_str());
                return -ENOENT;
            }

            DirectoryEntry ent;
            strcpy(ent.name, basename.c_str());

            IF_DEBUG(debugLevelSyscalls >= DebugLevelNormal, { Log::Info("SysOpen: Creating %s", filepath); });

            parent->Create(&ent, flags);

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
    String linkName = fs::BaseName(newpath);

    Log::Info("SysLink: Attempting to create link %s at path %s", linkName.c_str(), newpath);

    if (!parentDirectory) {
        Log::Warning("SysLink: Could not resolve path: %s", newpath);
        return -ENOENT;
    }

    if ((parentDirectory->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        Log::Warning("SysLink: Could not resolve path: Parent not a directory: %s", newpath);
        return -ENOTDIR;
    }

    DirectoryEntry entry;
    strcpy(entry.name, linkName.c_str());
    return parentDirectory->Link(file, &entry);
}

long SysUnlink(RegisterContext* r) {
    int fd = SC_ARG0(r);
    const char* path = (const char*)SC_ARG1(r);
    int flag = SC_ARG2(r);

    if (flag & AT_REMOVEDIR) {
        Log::Warning("SysUnlink: AT_REMOVEDIR unsupported!");
        return -EINVAL;
    }

    if (flag & ~(AT_REMOVEDIR)) {
        return -EINVAL;
    }

    Process* proc = Scheduler::GetCurrentProcess();

    if (!Memory::CheckUsermodePointer(SC_ARG0(r), 1, proc->addressSpace)) {
        Log::Warning("SysUnlink: Invalid path pointer");
        return -EFAULT;
    }

    FsNode* workingDir;
    if (fd == AT_FDCWD) {
        workingDir = proc->workingDir->node;
    } else {
        workingDir = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(fd))->node;
    }

    String linkName = fs::BaseName(path);

    if (!workingDir) {
        Log::Warning("SysUnlink: Could not resolve path: %s", path);
        return -EINVAL;
    }

    if (!workingDir->IsDirectory()) {
        Log::Warning("SysUnlink: Could not resolve path: Not a directory: %s", path);
        return -ENOTDIR;
    }

    DirectoryEntry entry;
    strcpy(entry.name, linkName.c_str());
    return workingDir->Unlink(&entry);
}

long SysChdir(RegisterContext* r) {
    Process* proc = Process::Current();
    if (SC_ARG0(r)) {
        String path = fs::CanonicalizePath((char*)SC_ARG0(r), proc->workingDirPath);
        FsNode* n = fs::ResolvePath(path);
        if (!n) {
            Log::Warning("chdir: Could not find %s", path.c_str());
            return -ENOENT;
        } else if (!n->IsDirectory()) {
            return -ENOTDIR;
        }

        strcpy(proc->workingDirPath, path.c_str());
        proc->workingDir = SC_TRY_OR_ERROR(n->Open(0));
    } else
        Log::Warning("chdir: Invalid path string");
    return 0;
}

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

long SysMount(RegisterContext* r) { return 0; }

long SysMkdir(RegisterContext* r) {
    char* path = (char*)SC_ARG0(r);
    mode_t mode = SC_ARG1(r);

    Process* proc = Scheduler::GetCurrentProcess();

    size_t pathLen;
    if (strlenSafe(path, pathLen, proc->addressSpace)) {
        Log::Warning("sys_mkdir: Invalid path pointer %x", SC_ARG0(r));
        return -EFAULT;
    }

    FsNode* parentDirectory = fs::ResolveParent(path, proc->workingDir->node);
    String dirPath = fs::BaseName(path);

    Log::Info("sys_mkdir: Attempting to create %s at path %s", dirPath.c_str(), path);

    if (!parentDirectory) {
        Log::Warning("sys_mkdir: Could not resolve path: %s", path);
        return -EINVAL;
    }

    if ((parentDirectory->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        Log::Warning("sys_mkdir: Could not resolve path: Not a directory: %s", path);
        return -ENOTDIR;
    }

    DirectoryEntry dir;
    strcpy(dir.name, dirPath.c_str());
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
    strcpy(ent.name, fs::BaseName(path).c_str());
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

    return fs::Rename(olddir, fs::BaseName(oldpath).c_str(), newdir, fs::BaseName(newpath).c_str());
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
    return fs::Write(handle->node, off, count, buffer);
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

    Handle h = process->GetHandle(SC_ARG0(r));
    if (h == HANDLE_NULL) {
        Log::Warning("SysIoctl: Invalid File Descriptor: %d", SC_ARG0(r));
        return -EBADF;
    }

    if (request == FIOCLEX) {
        h.closeOnExec = true;
        process->ReplaceHandle(SC_ARG0(r), std::move(h));

        *result = 0;
        return 0;
    } else if (request == FIONCLEX) {
        h.closeOnExec = false;
        process->ReplaceHandle(SC_ARG0(r), std::move(h));

        *result = 0;
        return 0;
    }

    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(process->GetHandleAs<UNIXOpenFile>(SC_ARG0(r)));
    if (!handle) {
        Log::Warning("SysIoctl: Not a UNIX file: %d", SC_ARG0(r));
        return -EBADF;
    }

    int ret = fs::Ioctl(handle, request, arg);

    if (result && ret >= 0) {
        *result = ret;
        return 0;
    }

    return ret;
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

    Thread* thread = Thread::Current();
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
    struct timespec* timeout = reinterpret_cast<struct timespec*>(SC_ARG4(r));

    if (!((!readFdsMask || Memory::CheckUsermodePointer(SC_ARG1(r), sizeof(fd_set_t), currentProcess->addressSpace)) &&
          (!writeFdsMask || Memory::CheckUsermodePointer(SC_ARG2(r), sizeof(fd_set_t), currentProcess->addressSpace)) &&
          (!exceptFdsMask ||
           Memory::CheckUsermodePointer(SC_ARG3(r), sizeof(fd_set_t), currentProcess->addressSpace)) &&
          Memory::CheckUsermodePointer(SC_ARG4(r), sizeof(struct timespec), currentProcess->addressSpace))) {
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

long SysFChdir(RegisterContext* r) {
    Process* process = Process::Current();
    int fd = SC_ARG0(r);

    auto dirHandle = SC_TRY_OR_ERROR(process->GetHandleAs<UNIXOpenFile>(fd));
    process->workingDir = dirHandle;

    return 0;
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
