#include <Fs/Filesystem.h>

#include <Errno.h>
#include <Logging.h>

FsNode::~FsNode(){
    
}

ErrorOr<ssize_t> FsNode::read(size_t, size_t, UIOBuffer*){
    Log::Warning("Base FsNode::Read called!");
    return Error{ENOSYS};
}

ErrorOr<ssize_t> FsNode::write(size_t, size_t, UIOBuffer*){
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

ErrorOr<int> FsNode::read_dir(DirectoryEntry*, uint32_t){
    if(type != FileType::Directory){
        return Error{ENOTDIR};
    }
    
    assert(!"Base FsNode::read_dir called!");
    return Error{ENOSYS};
}

ErrorOr<FsNode*> FsNode::find_dir(const char*){
    assert(is_directory());

    assert(!"Base FsNode::find_dir called!");
    return Error{ENOSYS};
}

Error FsNode::create(DirectoryEntry*, uint32_t){
    if(type != FileType::Directory){
        return Error{ENOTDIR};
    }

    assert("Base FsNode::Create called!");
    return Error{ENOSYS};
}

Error FsNode::create_directory(DirectoryEntry*, uint32_t){
    if(type != FileType::Directory){
        return Error{ENOTDIR};
    }

    assert("Base FsNode::create_directory called!");
    return Error{ENOSYS};
}

ErrorOr<ssize_t> FsNode::read_link(char* pathBuffer, size_t bufSize){
    if(!is_symlink()){
        return Error{EINVAL}; // Not a symlink
    }

    assert("Base FsNode::read_link called!");
    return Error{ENOSYS};
}

Error FsNode::link(FsNode*, DirectoryEntry*){
    assert(!"Base FsNode::Link called!");
    return Error{ENOSYS};
}

Error FsNode::unlink(DirectoryEntry*, bool unlinkDirs){
    assert(!"Base FsNode::Unlink called!");
    return Error{ENOSYS};
}

Error FsNode::truncate(off_t length){
    assert(!"Base FsNode::truncate called!");
    return Error{ENOSYS};
}

ErrorOr<int> FsNode::ioctl(uint64_t cmd, uint64_t arg){
    Log::Debug(debugLevelFilesystem, DebugLevelNormal, "Base FsNode::ioctl called! (cmd: %x)", cmd);
    return Error{ENOTTY};
}

ErrorOr<MappedRegion*> FsNode::mmap(uintptr_t base, uintptr_t size, off_t off, int prot, bool shared, bool fixed) {
    return ENODEV;
}

void FsNode::sync(){
    
}
