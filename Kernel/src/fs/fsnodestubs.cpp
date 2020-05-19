#include <fs/filesystem.h>

size_t FsNode::Read(size_t, size_t, uint8_t *){
    return 0;
}

size_t FsNode::Write(size_t, size_t, uint8_t *){
    return 0;
}

fs_fd_t* FsNode::Open(size_t flags){
    fs_fd_t* fDesc = (fs_fd_t*)kmalloc(sizeof(fs_fd_t));

    fDesc->pos = 0;
    fDesc->mode = flags;
    fDesc->node = this;

    return fDesc;
}

void FsNode::Close(){

}

int FsNode::ReadDir(struct fs_dirent*, uint32_t){
    return 0;
}

FsNode* FsNode::FindDir(char* name){
    return nullptr;
}

int FsNode::Ioctl(uint64_t cmd, uint64_t arg){
    return -1;
}