#include <fs/filesystem.h>

#include <errno.h>

FsNode::~FsNode(){
    
}

ssize_t FsNode::Read(size_t, size_t, uint8_t *){
    return -ENOSYS;
}

ssize_t FsNode::Write(size_t, size_t, uint8_t *){
    return -ENOSYS;
}

fs_fd_t* FsNode::Open(size_t flags){
    fs_fd_t* fDesc = (fs_fd_t*)kmalloc(sizeof(fs_fd_t));

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

int FsNode::Link(FsNode*, DirectoryEntry*){
    return -ENOSYS;
}

int FsNode::Unlink(DirectoryEntry*){
    return -ENOSYS;
}

int FsNode::Rename(FsNode*, DirectoryEntry*, FsNode*, DirectoryEntry*){
    return -ENOSYS;
}

int FsNode::Ioctl(uint64_t cmd, uint64_t arg){
    return -ENOSYS;
}

void FsNode::Sync(){
    
}