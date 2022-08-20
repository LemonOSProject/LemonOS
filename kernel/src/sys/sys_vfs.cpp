#include "syscall.h"

#include <Error.h>
#include <Fs/Filesystem.h>
#include <Logging.h>
#include <Objects/Process.h>

#include <Types.h>

#include <abi/ioctl.h>
#include <abi/lseek.h>
#include <abi/stat.h>

#define SC_TRY_OR_ERROR(func)                                                                                          \
    ({                                                                                                                 \
        auto result = func;                                                                                            \
        if (result.HasError()) {                                                                                       \
            return result.err.code;                                                                                    \
        }                                                                                                              \
        std::move(result.Value());                                                                                     \
    })

#define SC_TRY(func)                                                                                                   \
    ({                                                                                                                 \
        if (Error e = func; e.code)                                                                                    \
            return e.code;                                                                                             \
    })

#define FD_GET(handle)                                                                                                 \
    ({                                                                                                                 \
        auto r = Process::Current()->GetHandleAs<UNIXOpenFile>(handle);                                                \
        if (r.HasError()) {                                                                                            \
            return r.err.code;                                                                                         \
        }                                                                                                              \
        if (!r.Value()) {                                                                                              \
            return EBADF;                                                                                              \
        }                                                                                                              \
        std::move(r.Value());                                                                                          \
    })

#define SC_LOG_VERBOSE(msg, ...) ({ Log::Debug(debugLevelSyscalls, DebugLevelVerbose, msg, ##__VA_ARGS__); })

ALWAYS_INLINE void ReleaseRegions(Vector<MappedRegion*>& regions) {
    for (MappedRegion* r : regions) {
        r->lock.ReleaseWrite();
    }
}

SYSCALL long sys_read(le_handle_t handle, uint8_t* buf, size_t count, UserPointer<ssize_t> bytesRead) {
    Process* process = Process::Current();

    FancyRefPtr<UNIXOpenFile> fd = FD_GET(handle);

    UIOBuffer buffer = UIOBuffer::FromUser(buf);

    auto r = fs::Read(fd, count, &buffer);
    if (r.HasError()) {
        return r.err.code;
    }

    if (bytesRead.StoreValue(r.Value())) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_write(le_handle_t handle, const uint8_t* buf, size_t count, UserPointer<ssize_t> bytesWritten) {
    Process* process = Process::Current();

    FancyRefPtr<UNIXOpenFile> fd = FD_GET(handle);

    UIOBuffer buffer = UIOBuffer::FromUser((uint8_t*)buf);

    auto r = fs::Write(fd, count, &buffer);

    if (r.HasError()) {
        return r.err.code;
    }

    if (bytesWritten.StoreValue(r.Value())) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_openat(le_handle_t directory, le_str_t _filename, int flags, int mode, UserPointer<le_handle_t> out) {
    Process* process = Process::Current();
    String filename = get_user_string_or_fault(_filename);

    if (filename.Length() > PATH_MAX) {
        return ENAMETOOLONG;
    }

    bool followSymlinks = !(flags & O_NOFOLLOW);
    bool create = flags & O_CREAT;
    bool mustCreate = flags & O_EXCL;
    bool expectsDirectory = flags & O_DIRECTORY;
    bool truncate = flags & O_TRUNC;
    bool closeOnExec = flags & O_CLOEXEC;
    bool append = flags & O_APPEND;
    bool doNotSetAccessTime = flags & O_NOATIME;
    bool doNotSetAsCTTY = flags & O_NOCTTY;

    if (flags & O_PATH) {
        Log::Error("sys_openat: O_PATH not supported");
        return ENOSYS;
    }

    if (flags & O_DIRECT) {
        Log::Error("sys_openat: O_DIRECT not supported");
        return EINVAL;
    }

    if (flags & O_TMPFILE) {
        Log::Error("sys_openat: O_TMPFILE not supported");
        return ENOSYS;
    }

    if (doNotSetAsCTTY) {
        Log::Error("sys_openat: O_NOCTTY not supported");
        return ENOSYS;
    }

    int knownFlags = O_ACCESS | O_APPEND | O_CREAT | O_DIRECTORY | O_EXCL | O_NOCTTY | O_NOFOLLOW | O_TRUNC |
                     O_NONBLOCK | O_CLOEXEC | O_PATH | O_NOATIME | O_TMPFILE | O_DIRECT;
    if (flags & ~knownFlags) {
        Log::Error("sys_openat: Unknown flags %x", flags & ~knownFlags);
        return EINVAL;
    }

    // These flags will get applied to final file descriptor
    int fdFlags = flags & (O_NONBLOCK);

    if (append) {
        // For now O_APPEND only sets the file position when opened,
        // really it should be in 'append mode' where all writes occur at the end
        Log::Warning("sys_openat: O_APPEND not implmented correctly");
    }

    bool read = false;
    bool write = false;

    int access = flags & O_ACCESS;
    switch (access) {
    case O_RDONLY:
        read = true;
        break;
    case O_WRONLY:
        write = true;
        break;
    case O_RDWR:
        read = true;
        write = true;
        break;
    default:
        Log::Warning("sys_openat: Invalid access flags %d", access);
        return EINVAL;
    }

    FancyRefPtr<UNIXOpenFile> fd;
    FsNode* dirNode;

    if (filename[0] == '/') {
        // Path is absolute
        dirNode = nullptr;
    } else {
        if (directory == AT_FDCWD) {
            fd = process->workingDir;
        } else {
            fd = FD_GET(directory);
            if (!fd->node->IsDirectory()) {
                return ENOTDIR;
            }
        }

        assert(fd);

        if (!fd->node->IsDirectory()) {
            return ENOTDIR;
        }

        dirNode = fd->node;
    }

open:
    FsNode* file = fs::ResolvePath(filename, dirNode, followSymlinks);
    if (create) {
        if (mustCreate && file) {
            // Attempting to create a file that already exists
            return EEXIST;
        }

        FsNode* parent = fs::ResolveParent(filename, nullptr);
        if (!parent) {
            // Parent directory does not exist
            return ENOENT;
        }

        String basename = fs::BaseName(filename);

        DirectoryEntry dent;
        strcpy(dent.name, basename.c_str());

        if (int e = parent->Create(&dent, mode); e < 0) {
            return -e;
        }
        create = false;

        goto open; // Try open the file now
    } else if (!file) {
        return ENOENT;
    }

    if (doNotSetAccessTime && process->euid != file->uid) {
        Log::Warning("sys_open: File not owned by calling process (O_NOATIME specified)");
        return EPERM;
    }

    if (!followSymlinks && file->IsSymlink()) {
        Log::Warning("sys_open: File is a symlink (O_NOFOLLOW specified)");
        return ELOOP;
    }

    // Write access modes are incompatible with directories
    if (write && file->IsDirectory()) {
        return EISDIR;
    }

    if (expectsDirectory) {
        if (!file->IsDirectory()) {
            return ENOTDIR;
        }
    }

    auto openFile = SC_TRY_OR_ERROR(file->Open(flags));
    assert(openFile && !openFile->pos);

    le_handle_t handle = process->AllocateHandle(openFile, closeOnExec);
    if (out.StoreValue(handle)) {
        return EFAULT;
    }

    SC_LOG_VERBOSE("opened %s!", filename.c_str());

    return 0;
}

SYSCALL long sys_fstatat(int fd, le_str_t path, int flags, UserPointer<struct stat> out) {
    Process* process = Process::Current();

    FancyRefPtr<UNIXOpenFile> wd;
    if (fd == AT_FDCWD) {
        wd = process->workingDir;
    } else {
        wd = FD_GET(fd);
    }

    FsNode* node;
    if (flags & AT_EMPTY_PATH) {
        node = wd->node;
    } else {
        bool followSymlinks = !(flags & AT_SYMLINK_NOFOLLOW);

        node = fs::ResolvePath(path, wd->node, followSymlinks);
    }

    struct stat st;

    st.st_dev = node->volumeID;
    st.st_ino = node->inode;
    st.st_nlink = node->nlink;
    st.st_mode = node->pmask;
    st.st_uid = node->uid;
    st.st_gid = node->gid;
    st.st_rdev = 0;
    st.st_size = node->size;
    // TODO: access time
    // TODO: modification time
    // TODO: creation time

    // TODO: blksize
    st.st_blksize = 0;
    // TODO: blocks
    st.st_blocks = node->size;

    if (out.StoreValue(st)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_lseek(le_handle_t handle, off_t offset, unsigned int whence, UserPointer<off_t> out) {
    auto file = FD_GET(handle);

    if (file->node->IsSocket() || file->node->IsPipe()) {
        return ESPIPE;
    }

    ScopedSpinLock lockPosition(file->dataLock);

    off_t offsetResult = 0;

    switch (whence) {
    case SEEK_SET:
        offsetResult = offset;
        break;
    case SEEK_CUR:
        offsetResult = file->pos + offset;
        break;
    case SEEK_END:
        offsetResult = file->node->size + offset;
        break;
    default:
        return EINVAL;
    }

    if (offsetResult < 0) {
        // Resulting offset cannot be negative
        return EINVAL;
    }

    file->pos = offsetResult;

    // If the out pointer is null just ignore it
    if (out.Pointer() && out.StoreValue(offsetResult)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_ioctl(le_handle_t handle, unsigned int cmd, unsigned long arg, UserPointer<int> result) {
    Process* process = Process::Current();

    FancyRefPtr<UNIXOpenFile> fd = FD_GET(handle);

    long r = 0;

    switch (cmd) {
    case FIOCLEX:
        SC_TRY(process->SetCloseOnExec(handle, 1));
        break;
    case FIONCLEX:
        SC_TRY(process->SetCloseOnExec(handle, 0));
        break;
    default:
        r = SC_TRY_OR_ERROR(fs::Ioctl(fd, cmd, arg));
        break;
    }

    if (result.StoreValue(r)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_pread(le_handle_t handle, void* buf, size_t count, off_t pos, UserPointer<ssize_t> bytes) {
    Process* process = Process::Current();

    FancyRefPtr<UNIXOpenFile> fd = FD_GET(handle);

    UIOBuffer uio = UIOBuffer::FromUser(buf);
    auto r = fs::Read(fd, count, &uio, pos);

    if (r.HasError()) {
        return r.err.code;
    }

    if (bytes.StoreValue(r.Value())) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_pwrite(le_handle_t handle, const void* buf, size_t count, off_t pos, UserPointer<ssize_t> bytes) {
    Process* process = Process::Current();

    FancyRefPtr<UNIXOpenFile> fd = FD_GET(handle);

    UIOBuffer uio = UIOBuffer::FromUser((void*)buf);
    auto r = fs::Write(fd, count, &uio, pos);

    if (r.HasError()) {
        return r.err.code;
    }

    if (bytes.StoreValue(r.Value())) {
        return EFAULT;
    }

    return 0;
}
