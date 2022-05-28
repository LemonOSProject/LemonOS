#include <Fs/Filesystem.h>

#include <Errno.h>
#include <Logging.h>

FsNode::~FsNode(){
    
}

ssize_t FsNode::Read(size_t, size_t, uint8_t *){
    Log::Warning("Base FsNode::Read called!");
    return -ENOSYS;
}

ssize_t FsNode::Write(size_t, size_t, uint8_t *){
    Log::Warning("Base FsNode::Write called!");
    return -ENOSYS;
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

int FsNode::ReadDir(DirectoryEntry*, uint32_t){
    if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
        return -ENOTDIR;
    }
    
    assert(!"Base FsNode::ReadDir called!");
    return -ENOSYS;
}

FsNode* FsNode::FindDir(const char*){
    assert(IsDirectory());

    assert(!"Base FsNode::FindDir called!");
    return nullptr;
}

int FsNode::Create(DirectoryEntry*, uint32_t){
    if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
        return -ENOTDIR;
    }

    assert("Base FsNode::Create called!");
    return -ENOSYS;
}

int FsNode::CreateDirectory(DirectoryEntry*, uint32_t){
    if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
        return -ENOTDIR;
    }

    assert("Base FsNode::CreateDirectory called!");
    return -ENOSYS;
}

ssize_t FsNode::ReadLink(char* pathBuffer, size_t bufSize){
    if((flags & S_IFMT) != S_IFLNK){
        return -EINVAL; // Not a symlink
    }

    assert("Base FsNode::ReadLink called!");
    return -ENOSYS;
}

int FsNode::Link(FsNode*, DirectoryEntry*){
    assert(!"Base FsNode::Link called!");
    return -ENOSYS;
}

int FsNode::Unlink(DirectoryEntry*, bool unlinkDirs){
    assert(!"Base FsNode::Unlink called!");
    return -ENOSYS;
}

int FsNode::Truncate(off_t length){
    assert(!"Base FsNode::Truncate called!");
    return -ENOSYS;
}

int FsNode::Ioctl(uint64_t cmd, uint64_t arg){
    Log::Debug(debugLevelFilesystem, DebugLevelNormal, "Base FsNode::Ioctl called! (cmd: %x)", cmd);
    return -ENOSYS;
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