#include <Objects/File.h>

#include <Fs/Filesystem.h>
#include <Logging.h>

#include <abi/ioctl.h>

ALWAYS_INLINE File::~File() {}

ErrorOr<ssize_t> File::Read(off_t, size_t, UIOBuffer*) {
    Log::Warning("Base File::Read");
    return EINVAL;
}

ErrorOr<ssize_t> File::Write(off_t, size_t, UIOBuffer*) {
    Log::Warning("Base File::Write");
    return EINVAL;
}

ErrorOr<ssize_t> File::Read(size_t size, UIOBuffer* buffer) {
    auto len = TRY_OR_ERROR(Read(pos, size, buffer));
    
    pos += len;
    return len;
}

ErrorOr<ssize_t> File::Write(size_t size, UIOBuffer* buffer) {
    auto len = TRY_OR_ERROR(Write(pos, size, buffer));
    
    pos += len;
    return len;
}

ErrorOr<int> File::Ioctl(uint64_t cmd, uint64_t arg) {
    Log::Warning("Base File::Ioctl");

    // These should have been handled by the syscall handler
    assert(cmd != FIONCLEX);
    assert(cmd != FIOCLEX);

    // Invalid request for file
    return ENOTTY;
}

void File::Watch(FsWatcher*, Fs::FsEvent) {}
void File::Unwatch(FsWatcher*) {}

ErrorOr<int> File::ReadDir(struct DirectoryEntry*, uint32_t) {
    if (!IsDirectory()) {
        return ENOTDIR;
    }

    return ENOSYS;
}

ErrorOr<class MappedRegion*> File::MMap(uintptr_t base, size_t size, off_t off, int prot, bool shared, bool fixed) {
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

ErrorOr<ssize_t> NodeFile::Read(off_t off, size_t size, UIOBuffer* buffer) {
    return inode->Read(off, size, buffer);
}

ErrorOr<ssize_t> NodeFile::Write(off_t off, size_t size, UIOBuffer* buffer) {
    return inode->Write(off, size, buffer);
}

ErrorOr<int> NodeFile::Ioctl(uint64_t cmd, uint64_t arg) {
    return inode->Ioctl(cmd, arg);
}

ErrorOr<int> NodeFile::ReadDir(struct DirectoryEntry* d, uint32_t index) { return inode->ReadDir(d, index); }

ErrorOr<class MappedRegion*> NodeFile::MMap(uintptr_t base, size_t size, off_t off, int prot, bool shared, bool fixed) {
    return inode->MMap(base, size, off, prot, shared, fixed);
}
