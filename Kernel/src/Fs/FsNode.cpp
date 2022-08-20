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

ErrorOr<UNIXOpenFile*> FsNode::Open(size_t flags){
    UNIXOpenFile* fDesc = new UNIXOpenFile;

    fDesc->pos = 0;
    fDesc->mode = flags;
    fDesc->node = this;

    handleCount++;

    return fDesc;
}

void FsNode::Close(){
    handleCount--;
}

ErrorOr<int> FsNode::ReadDir(DirectoryEntry*, uint32_t){
    if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
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
    if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
        return Error{ENOTDIR};
    }

    assert("Base FsNode::Create called!");
    return Error{ENOSYS};
}

Error FsNode::CreateDirectory(DirectoryEntry*, uint32_t){
    if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
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

void FsNode::Watch(FilesystemWatcher& watcher, int events){
    Log::Warning("FsNode::Watch base called");
    
    watcher.Signal();
}

void FsNode::Unwatch(FilesystemWatcher& watcher){
    Log::Warning("FsNode::Unwatch base called");
}

void FsNode::Sync(){
    
}

void FsNode::UnblockAll(){
    acquireLock(&blockedLock);
    while(blocked.get_length()){
        blocked.get_front()->Unblock();
    }
    releaseLock(&blockedLock);
}