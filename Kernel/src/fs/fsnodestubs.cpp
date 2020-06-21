#include <fs/filesystem.h>

FsNode::~FsNode(){
    
}

ssize_t FsNode::Read(size_t, size_t, uint8_t *){
    return 0;
}

ssize_t FsNode::Write(size_t, size_t, uint8_t *){
    return 0;
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
    return -1;
}

FsNode* FsNode::FindDir(char* name){
    return nullptr;
}

int FsNode::Create(DirectoryEntry*, uint32_t){
    return -1;
}

int FsNode::CreateDirectory(DirectoryEntry*, uint32_t){
    return -1;
}

int FsNode::Link(FsNode*, DirectoryEntry*){
    return -1;
}

int FsNode::Unlink(DirectoryEntry*){
    return -1;
}

int FsNode::Ioctl(uint64_t cmd, uint64_t arg){
    return -1;
}

void FsNode::Sync(){
    
}