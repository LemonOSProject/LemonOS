#include <Fs/Filesystem.h>

#include <Errno.h>
#include <Logging.h>

FsNode::~FsNode(){
    
}

ErrorOr<ssize_t> FsNode::Read(size_t, size_t, UIOBuffer*){
    Log::Warning("Base FsNode::Read called!");
    return Error{ENOSYS};
}

ErrorOr<ssize_t> FsNode::Write(size_t, size_t, UIOBuffer*){
    Log::Warning("Base FsNode::Write called!");
    return Error{ENOSYS};
}

ErrorOr<File*> FsNode::Open(size_t flags){
    File* file = TRY_OR_ERROR(NodeFile::Create(this));

    handleCount++;

    return file;
}

void FsNode::Close(){
    handleCount--;
}

ErrorOr<int> FsNode::ReadDir(DirectoryEntry*, uint32_t){
    if(type != FileType::Directory){
        return Error{ENOTDIR};
    }
    
    assert(!"Base FsNode::ReadDir called!");
    return Error{ENOSYS};
}

ErrorOr<FsNode*> FsNode::FindDir(const char*){
    assert(IsDirectory());

    assert(!"Base FsNode::FindDir called!");
    return Error{ENOSYS};
}

Error FsNode::Create(DirectoryEntry*, uint32_t){
    if(type != FileType::Directory){
        return Error{ENOTDIR};
    }

    assert("Base FsNode::Create called!");
    return Error{ENOSYS};
}

Error FsNode::CreateDirectory(DirectoryEntry*, uint32_t){
    if(type != FileType::Directory){
        return Error{ENOTDIR};
    }

    assert("Base FsNode::CreateDirectory called!");
    return Error{ENOSYS};
}

ErrorOr<ssize_t> FsNode::ReadLink(char* pathBuffer, size_t bufSize){
    if(!IsSymlink()){
        return Error{EINVAL}; // Not a symlink
    }

    assert("Base FsNode::ReadLink called!");
    return Error{ENOSYS};
}

Error FsNode::Link(FsNode*, DirectoryEntry*){
    assert(!"Base FsNode::Link called!");
    return Error{ENOSYS};
}

Error FsNode::Unlink(DirectoryEntry*, bool unlinkDirs){
    assert(!"Base FsNode::Unlink called!");
    return Error{ENOSYS};
}

Error FsNode::Truncate(off_t length){
    assert(!"Base FsNode::Truncate called!");
    return Error{ENOSYS};
}

ErrorOr<int> FsNode::Ioctl(uint64_t cmd, uint64_t arg){
    Log::Debug(debugLevelFilesystem, DebugLevelNormal, "Base FsNode::Ioctl called! (cmd: %x)", cmd);
    return Error{ENOTTY};
}

ErrorOr<MappedRegion*> FsNode::MMap(uintptr_t base, uintptr_t size, off_t off, int prot, bool shared, bool fixed) {
    return ENODEV;
}

void FsNode::Sync(){
    
}