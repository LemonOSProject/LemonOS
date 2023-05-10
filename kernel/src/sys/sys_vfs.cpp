#include "syscall.h"

#include <Fs/Filesystem.h>

#include <Objects/Process.h>

#include <Error.h>
#include <Logging.h>
#include <OnReturn.h>

#include <Types.h>

#include <abi/dirent.h>
#include <abi/ioctl.h>
#include <abi/lseek.h>
#include <abi/poll.h>
#include <abi/stat.h>

SYSCALL long sys_read(le_handle_t handle, uint8_t* buf, size_t count, UserPointer<ssize_t> bytesRead) {
    Process* process = Process::current();

    FancyRefPtr<File> fd = FD_GET(handle);

    UIOBuffer buffer = UIOBuffer::from_user(buf);

    auto r = fd->read(count, &buffer);
    if (r.HasError()) {
        return r.err.code;
    }

    if (bytesRead.store(r.Value())) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_write(le_handle_t handle, const uint8_t* buf, size_t count, UserPointer<ssize_t> bytesWritten) {
    Process* process = Process::current();

    FancyRefPtr<File> fd = FD_GET(handle);

    UIOBuffer buffer = UIOBuffer::from_user((uint8_t*)buf);

    auto r = fd->write(count, &buffer);
    if (r.HasError()) {
        return r.err.code;
    }

    if (bytesWritten.store(r.Value())) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_openat(le_handle_t directory, le_str_t _filename, int flags, int mode, UserPointer<le_handle_t> out) {
    Process* process = Process::current();
    String filename = get_user_string_or_fault(_filename, PATH_MAX + 1);

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
        Log::Error("sys_openat: TTY is never actually set as controlling TTY");
        // return ENOSYS;
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

    FancyRefPtr<File> fd;
    FsNode* dirNode;

    if (filename[0] == '/') {
        // Path is absolute
        dirNode = nullptr;
    } else {
        if (directory == AT_FDCWD) {
            fd = process->workingDir;
        } else {
            fd = FD_GET(directory);
            if (!fd->is_directory()) {
                return ENOTDIR;
            }
        }

        assert(fd);

        if (!fd->is_directory()) {
            return ENOTDIR;
        }

        dirNode = fd->inode;
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

        if (int e = parent->create(&dent, mode); e < 0) {
            return -e;
        }
        create = false;

        goto open; // Try open the file now
    } else if (!file) {
        SC_LOG_VERBOSE("sys_open: no such file '%s'", filename.c_str());
        return ENOENT;
    }

    if (doNotSetAccessTime && process->euid != file->uid) {
        Log::Warning("sys_open: File not owned by calling process (O_NOATIME specified)");
        return EPERM;
    }

    if (!followSymlinks && file->is_symlink()) {
        Log::Warning("sys_open: File is a symlink (O_NOFOLLOW specified)");
        return ELOOP;
    }

    // Write access modes are incompatible with directories
    if (write && file->is_directory()) {
        return EISDIR;
    }

    if (expectsDirectory) {
        if (!file->is_directory()) {
            return ENOTDIR;
        }
    }

    auto openFile = SC_TRY_OR_ERROR(file->Open(flags));
    assert(openFile && !openFile->pos);

    le_handle_t handle = process->allocate_handle(openFile, closeOnExec);
    if (out.store(handle)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_fstatat(int fd, le_str_t path, int flags, UserPointer<struct stat> out) {
    Process* process = Process::current();

    FancyRefPtr<File> wd;
    if (fd == AT_FDCWD) {
        wd = process->workingDir;
    } else {
        wd = FD_GET(fd);
    }

    FsNode* node;
    assert(wd->inode);

    if (flags & AT_EMPTY_PATH) {
        node = wd->inode;
    } else {
        if (!path) {
            return ENOENT;
        }

        bool followSymlinks = !(flags & AT_SYMLINK_NOFOLLOW);

        node = fs::ResolvePath(path, wd->inode, followSymlinks);
        if(!node) {
            return ENOENT;
        }
    }

    struct stat st;

    st.st_dev = node->volumeID;
    st.st_ino = node->inode;
    st.st_nlink = node->nlink;
    st.st_mode = node->pmask | ((mode_t)node->type);
    st.st_uid = node->uid;
    st.st_gid = node->gid;
    st.st_rdev = 0;
    st.st_size = node->size;
    // TODO: access time
    st.st_atim = {0, 0};
    // TODO: modification time
    st.st_mtim = {0, 0};
    // TODO: creation time
    st.st_ctim = {0, 0};

    // TODO: blksize
    st.st_blksize = 0;
    // TODO: blocks
    st.st_blocks = node->size;

    if (out.store(st)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_lseek(le_handle_t handle, off_t offset, unsigned int whence, UserPointer<off_t> out) {
    auto file = FD_GET(handle);

    if (file->is_socket() || file->is_pipe()) {
        return ESPIPE;
    }

    ScopedSpinLock lockPosition(file->fLock);

    off_t offsetResult = 0;

    switch (whence) {
    case SEEK_SET:
        offsetResult = offset;
        break;
    case SEEK_CUR:
        offsetResult = file->pos + offset;
        break;
    case SEEK_END:
        offsetResult = file->inode->size + offset;
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
    if (out.ptr() && out.store(offsetResult)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_ioctl(le_handle_t handle, unsigned int cmd, unsigned long arg, UserPointer<int> result) {
    Process* process = Process::current();

    FancyRefPtr<File> fd = FD_GET(handle);

    long r = 0;

    switch (cmd) {
    case FIOCLEX:
        SC_TRY(process->handle_set_cloexec(handle, 1));
        break;
    case FIONCLEX:
        SC_TRY(process->handle_set_cloexec(handle, 0));
        break;
    default:
        r = SC_TRY_OR_ERROR(fd->ioctl(cmd, arg));
        break;
    }

    if (result.store(r)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_pread(le_handle_t handle, void* buf, size_t count, off_t pos, UserPointer<ssize_t> bytes) {
    Process* process = Process::current();

    FancyRefPtr<File> fd = FD_GET(handle);

    UIOBuffer uio = UIOBuffer::from_user(buf);

    auto r = SC_TRY_OR_ERROR(fd->read(pos, count, &uio));
    if (bytes.store(r)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_pwrite(le_handle_t handle, const void* buf, size_t count, off_t pos, UserPointer<ssize_t> bytes) {
    Process* process = Process::current();

    FancyRefPtr<File> fd = FD_GET(handle);

    UIOBuffer uio = UIOBuffer::from_user((void*)buf);

    auto r = SC_TRY_OR_ERROR(fd->write(pos, count, &uio));
    if (bytes.store(r)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_poll(UserPointer<int> _events, UserBuffer<pollfd> fds, size_t nfds, long timeout, sigset_t sigset) {
    Process* process = Process::current();
    Thread* thread = Thread::current();

    // TODO: rlimits, limit the amount of files that can be passed to sys_poll and other syscalls

    uint64_t oldmask = thread->signalMask;
    if (sigset) {
        thread->signalMask = sigset;
    }

    OnReturn signals{[thread, oldmask]() -> void { thread->signalMask = oldmask; }};

    // How many elements in fds have events
    unsigned events = 0;

    struct sys_poll_fd {
        FancyRefPtr<KernelObject> o;
        pollfd fd;
        unsigned index;
    };

    List<sys_poll_fd> objects;
    for (unsigned i = 0; i < nfds; i++) {
        pollfd fd;

        if (fds.get(i, fd)) {
            return EFAULT;
        }

        fd.revents = 0;

        FancyRefPtr<KernelObject> obj = nullptr;
        if (fd.fd >= 0) {
            Handle h = process->get_handle(fd.fd);

            // If the handle is invalid, return POLLNVAL
            if (h.IsValid()) {
                obj = std::move(h.ko);

                KOEvent e = obj->poll_events((KOEvent)fd.events);
                if (e != KOEvent::None) {
                    fd.revents = (short)e;
                    events++;
                }
            } else {
                fd.revents |= POLLNVAL;
                events++;
            }
        }

        if (obj == nullptr || fd.revents) {
            if (fds.store(i, fd)) {
                return EFAULT;
            }
        } else {
            objects.add_back(sys_poll_fd{std::move(obj), fd, i});
        }
    }

    if (events) {
        TRY_STORE_UMODE_VALUE(_events, events);
        return 0;
    }

    KernelObjectWatcher w;
    for (const auto& o : objects) {
        o.o->Watch(&w, (KOEvent)o.fd.events);
    }

    long t = timeout;
    if (w.wait_timeout(t)) {
        return EINTR;
    }

    // Poll for events in each file descriptor
    for (auto& o : objects) {
        pollfd& fd = o.fd;

        fd.revents = (short)o.o->poll_events((KOEvent)fd.events);
        if (fd.revents) {
            events++;
        }

        if (fds.store(o.index, fd)) {
            return EFAULT;
        }
    }

    TRY_STORE_UMODE_VALUE(_events, events);
    return 0;
}

SYSCALL long sys_chdir(le_str_t wd) {
    Process* process = Process::current();

    // TODO: working directory race condition
    String path = get_user_string_or_fault(wd, PATH_MAX);

    Log::Error("sys_chdir is a stub!");
    return ENOSYS;
}

SYSCALL long sys_getcwd(void* cwd, size_t size) {
    Process* process = Process::current();

    // TODO: sys_getcwd should fail when:
    // Permission to read a path component was denied (EACCES)
    // The working directory has been unlinked (ENOENT)

    if (size == 0) {
        return EINVAL;
    }

    // TODO: working directory race condition
    // chdir and getcwd could get called at the same time by different threads
    size_t wdLen = strlen(process->workingDirPath);
    if (wdLen + 1 > size) {
        return ERANGE;
    }

    if (user_memcpy(cwd, process->workingDirPath, wdLen + 1)) {
        return EFAULT;
    }

    return 0;
}

SYSCALL long sys_readdir(le_handle_t fd, UserBuffer<uint8_t> d, size_t count, UserPointer<size_t> _bytes_read) {
    auto file = FD_GET(fd);
    if (!file->is_directory()) {
        return ENOTDIR;
    }

    FsNode* inode = file->inode;
    assert(inode);

    DirectoryEntry ent;

    ssize_t written = 0;
    ssize_t offsetOfNext = 0;
    while (written + sizeof(le_dirent) <= count) {
        int r = SC_TRY_OR_ERROR(inode->read_dir(&ent, file->pos));
        if (r == 0) {
            break; // No more entries!
        }

        // Align offset on 4 bytes
        size_t offset = offsetOfNext;
        uint16_t namelen = strlen(ent.name) + 1;
        uint16_t reclen = offsetof(le_dirent, d_name) + namelen;
        reclen = (reclen + 0x3) & (~0x3ULL);
        offsetOfNext += reclen;

        if (offset + reclen > count) {
            break;
        }

        // TODO: consider having DirectoryEntry just have le_dirent sitting inside
        le_dirent userEntry = {
            .d_ino = ent.ino, .d_off = file->pos, .d_reclen = reclen, .d_type = (uint8_t)(ent.flags & 0xff)};

        if (d.write((uint8_t*)&userEntry, offset, offsetof(le_dirent, d_name))) {
            return EFAULT;
        }

        if (d.write((uint8_t*)ent.name, offset + offsetof(le_dirent, d_name), namelen)) {
            return EFAULT;
        }
        file->pos++;
        written += reclen;
    }

    if (_bytes_read.store(written))
        return EFAULT;

    return 0;
}
