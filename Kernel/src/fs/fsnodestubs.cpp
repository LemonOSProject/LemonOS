#include <fs/filesystem.h>

#include <errno.h>
#include <logging.h>

FsNode::~FsNode(){
    
}

ssize_t FsNode::Read(size_t, size_t, uint8_t *){
    return -ENOSYS;
}

ssize_t FsNode::Write(size_t, size_t, uint8_t *){
    return -ENOSYS;
}

fs_fd_t* FsNode::Open(size_t flags){
    fs_fd_t* fDesc = new fs_fd_t;

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
    return -ENOSYS;
}

FsNode* FsNode::FindDir(char* name){
    return nullptr;
}

int FsNode::Create(DirectoryEntry*, uint32_t){
    return -ENOSYS;
}

int FsNode::CreateDirectory(DirectoryEntry*, uint32_t){
    return -ENOSYS;
}

ssize_t FsNode::ReadLink(char* pathBuffer, size_t bufSize){
    if((flags & S_IFMT) != S_IFLNK){
        return -EINVAL; // Not a symlink
    }

    return -ENOSYS;
}

int FsNode::Link(FsNode*, DirectoryEntry*){
    return -ENOSYS;
}

int FsNode::Unlink(DirectoryEntry*, bool unlinkDirs){
    return -ENOSYS;
}

int FsNode::Truncate(off_t length){
    return -ENOSYS;
}

int FsNode::Ioctl(uint64_t cmd, uint64_t arg){
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