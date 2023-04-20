#include <Objects/File.h>

#include <Fs/Filesystem.h>
#include <Logging.h>

#include <abi/ioctl.h>

ALWAYS_INLINE File::~File() {}

ErrorOr<ssize_t> File::read(off_t, size_t, UIOBuffer*) {
    Log::Warning("Base File::Read");
    return EINVAL;
}

ErrorOr<ssize_t> File::write(off_t, size_t, UIOBuffer*) {
    Log::Warning("Base File::Write");
    return EINVAL;
}

ErrorOr<ssize_t> File::read(size_t size, UIOBuffer* buffer) {
    auto len = TRY_OR_ERROR(read(pos, size, buffer));
    
    pos += len;
    return len;
}

ErrorOr<ssize_t> File::write(size_t size, UIOBuffer* buffer) {
    auto len = TRY_OR_ERROR(write(pos, size, buffer));
    
    pos += len;
    return len;
}

ErrorOr<int> File::ioctl(uint64_t cmd, uint64_t arg) {
    Log::Warning("Base File::ioctl");

    // These should have been handled by the syscall handler
    assert(cmd != FIONCLEX);
    assert(cmd != FIOCLEX);

    // Invalid request for file
    return ENOTTY;
}

void File::Watch(KernelObjectWatcher*, KOEvent) {}
void File::Unwatch(KernelObjectWatcher*) {}

ErrorOr<int> File::read_dir(struct DirectoryEntry*, uint32_t) {
    if (!is_directory()) {
        return ENOTDIR;
    }

    return ENOSYS;
}

ErrorOr<class MappedRegion*> File::mmap(uintptr_t base, size_t size, off_t off, int prot, bool shared, bool fixed) {
    return ENODEV;
}

ErrorOr<NodeFile*> NodeFile::Create(class FsNode* ino) {
    NodeFile* file = new NodeFile;

    file->inode = ino;
    file->type = ino->type;

    return file;
}

NodeFile::~NodeFile() {
    assert(inode);

    inode->Close();
}

ErrorOr<ssize_t> NodeFile::read(off_t off, size_t size, UIOBuffer* buffer) {
    return inode->read(off, size, buffer);
}

ErrorOr<ssize_t> NodeFile::write(off_t off, size_t size, UIOBuffer* buffer) {
    return inode->write(off, size, buffer);
}

ErrorOr<int> NodeFile::ioctl(uint64_t cmd, uint64_t arg) {
    return inode->ioctl(cmd, arg);
}

ErrorOr<int> NodeFile::read_dir(struct DirectoryEntry* d, uint32_t index) { return inode->read_dir(d, index); }

ErrorOr<class MappedRegion*> NodeFile::mmap(uintptr_t base, size_t size, off_t off, int prot, bool shared, bool fixed) {
    return inode->mmap(base, size, off, prot, shared, fixed);
}
