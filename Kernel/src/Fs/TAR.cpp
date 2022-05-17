#include <Fs/TAR.h>

#include <Logging.h>
#include <Errno.h>

inline static long OctToDec(char* str, int size) {
    long n = 0;
    while (size-- && *str) {
        n <<= 3;
        n |= (*str - '0') & 0x7;
        str++;
    }
    return n;
}

inline static long GetSize(char* size){ // Get size of file in blocks
    long sz = OctToDec(size, 12);
    return sz;
}

inline static long GetBlockCount(char* size){ // Get size of file in blocks
    long sz = OctToDec(size, 12);
    return (sz + 511) / 512;
}

inline static uint32_t TarTypeToFilesystemFlags(char type){
    switch(type){
        case TAR_TYPE_DIRECTORY:
            return FS_NODE_DIRECTORY;
        default:
            return FS_NODE_FILE;
    }
}

namespace fs::tar{
    ssize_t TarNode::Read(size_t offset, size_t size, uint8_t *buffer){
        if(vol){
            return vol->Read(this, offset, size, buffer);
        } else return -1;
    }

    ssize_t TarNode::Write(size_t offset, size_t size, uint8_t *buffer){
        if(vol){
            return vol->Write(this, offset, size, buffer);
        } else return -1;
    }

    void TarNode::Close(){
        if(vol){
            vol->Close(this);
        }
    }
    int TarNode::ReadDir(DirectoryEntry* dirent, uint32_t index){
        if(vol){
            return vol->ReadDir(this, dirent, index);
        } else return -10;
    }
    FsNode* TarNode::FindDir(const char* name){
        if(vol){
            return vol->FindDir(this, name);
        } else return nullptr;
    }

    void TarVolume::MakeNode(tar_header_t* header, TarNode* n, ino_t inode, ino_t parent, tar_header_t* dirHeader){
        n->parentInode = parent;
        n->header = header;

        n->inode = inode;
        n->uid = OctToDec(header->ustar.uid, 8);
        n->flags = TarTypeToFilesystemFlags(header->ustar.type);
        n->vol = this;
        n->volumeID = volumeID;

        char* strtokSavePtr;
        char* name = header->ustar.name;
        char* _name = strtok_r(header->ustar.name, "/", &strtokSavePtr);
        while(_name){
            name = _name;
            _name = strtok_r(NULL, "/", &strtokSavePtr);
        }
        strcpy(n->name, name);
        n->size = GetSize(header->ustar.size);
    }

    int TarVolume::ReadDirectory(int blockIndex, ino_t parent){
        ino_t dirInode = nextNode++;
        tar_header_t* dirHeader = &blocks[blockIndex];
        TarNode* dirNode = &nodes[dirInode];
        MakeNode(dirHeader, dirNode, dirInode, parent);
        dirNode->entryCount = 0;

        unsigned i = blockIndex + GetBlockCount(dirHeader->ustar.size) + 1; // Next block
        while(i < blockCount){
            if(strncmp(blocks[i].ustar.name, dirHeader->ustar.name, strlen(dirHeader->ustar.name)) || !strlen(blocks[i].ustar.name)){
                break; // End of directory - header is not in directory
            } else if(blocks[i].ustar.name[strlen(dirHeader->ustar.name)] != '/'){ // Check for the path separator
                break; // Wwe encountered a file with a name starting with the name of the directory
            }

            dirNode->entryCount++;
            i += GetBlockCount(blocks[i].ustar.size) + 1;
        }
        
        dirNode->children = (ino_t*)kmalloc(sizeof(ino_t) * dirNode->entryCount);

        i = blockIndex + GetBlockCount(dirHeader->ustar.size) + 1;
        for(int e = 0; i < blockCount && e < dirNode->entryCount; e++){ // Iterate through directory
            ino_t inode = nextNode++;
            TarNode* n = &nodes[inode];
            MakeNode(&blocks[i], n, inode, dirInode, dirHeader);
            dirNode->children[e] = inode;

            //Log::Info("[TAR] Found File: %s, ", blocks[i].ustar.name);

            i += GetBlockCount(blocks[i].ustar.size) + 1;
        }

        return i;
    }

    TarVolume::TarVolume(uintptr_t base, size_t size, char* name){
        blocks = (tar_header_t*)base;
        blockCount = size / 512;

        int entryCount = 0;
        for(uint64_t i = 0; i < blockCount; i++, nodeCount++){ // Get file count
            tar_header_t header = blocks[i];

            if(!strchr(header.ustar.name, '/') || (header.ustar.type == TAR_TYPE_DIRECTORY && strchr(header.ustar.name, '/') == header.ustar.name + strlen(header.ustar.name) - 1)) entryCount++;

            i += GetBlockCount(header.ustar.size);
        }

        nodes = new TarNode[nodeCount];

        TarNode* volumeNode = &nodes[0];
        volumeNode->header = nullptr;
        volumeNode->flags = FS_NODE_DIRECTORY | FS_NODE_MOUNTPOINT;
        volumeNode->inode = 0;
        volumeNode->size = size;
        volumeNode->vol = this;
        volumeNode->parent = 0;

        mountPoint = volumeNode;
        strcpy(mountPointDirent.name, name);
        mountPointDirent.flags = DT_DIR;
        mountPointDirent.node = volumeNode;

        volumeNode->children = (ino_t*)kmalloc(sizeof(ino_t) * entryCount);
        volumeNode->entryCount = entryCount;
        int e = 0;
        for(unsigned i = 0; i < blockCount; e++){
            tar_header_t header = blocks[i];
            
            if(!strlen(header.ustar.name)) break;

            if(header.ustar.type == TAR_TYPE_DIRECTORY){
                volumeNode->children[e] = nextNode; // Directory will take next available node so add it to children
                i = ReadDirectory(i, 0);
                continue;
            } else if(header.ustar.type == TAR_TYPE_FILE) {
                ino_t inode = nextNode++;
                TarNode* node = &nodes[inode];
                MakeNode(&blocks[i], node, inode, 0);
                volumeNode->children[e] = inode;
            }

            i += GetBlockCount(header.ustar.size);
            i++;
        }
        volumeNode->entryCount = e;
    }

    ssize_t TarVolume::Read(TarNode* node, size_t offset, size_t size, uint8_t *buffer){
        TarNode* tarNode = &nodes[node->inode];

        if((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
            return -EISDIR;
        }

		if(offset > node->size) return 0;
		else if(offset + size > node->size) size = node->size - offset;

		if(!size) return 0;

		memcpy(buffer, (void*)(((uintptr_t)tarNode->header) + 512 + offset), size);
		return size;
    }

    ssize_t TarVolume::Write(TarNode* node, size_t offset, size_t size, uint8_t *buffer){
        if((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
            return -EISDIR;
        }

        return -EROFS;
    }

    void TarVolume::Open(__attribute__((unused)) TarNode* node, __attribute__((unused)) uint32_t flags){

    }

    void TarVolume::Close(__attribute__((unused)) TarNode* node){

    }

    int TarVolume::ReadDir(TarNode* node, DirectoryEntry* dirent, uint32_t index){
        TarNode* tarNode = &nodes[node->inode];
        
        if((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) return -ENOTDIR;

        if(index >= static_cast<unsigned>(tarNode->entryCount + 2)) return 0;

        if(index == 0){
            strcpy(dirent->name, ".");
            dirent->flags = DT_DIR;
            return 1;
        } else if(index == 1){
            strcpy(dirent->name, "..");
            dirent->flags = DT_DIR;
            return 1;
        }

        TarNode* dir = &nodes[tarNode->children[index - 2]];

        strcpy(dirent->name, dir->name);
        dirent->flags = dir->flags;
        dirent->node = dir;

        switch(dir->flags & FS_NODE_TYPE){
            case FS_NODE_FILE:
                dirent->flags = DT_REG;
                break;
            case FS_NODE_DIRECTORY:
                dirent->flags = DT_DIR;
                break;
            case FS_NODE_CHARDEVICE:
                dirent->flags = DT_CHR;
                break;
            case FS_NODE_BLKDEVICE:
                dirent->flags = DT_BLK;
                break;
            case FS_NODE_SOCKET:
                dirent->flags = DT_SOCK;
                break;
            case FS_NODE_SYMLINK:
                dirent->flags = DT_LNK;
                break;
        }

        return 1;
    }

    FsNode* TarVolume::FindDir(TarNode* node, const char* name){
        TarNode* tarNode = &nodes[node->inode];
        
        if(!(node->flags & FS_NODE_DIRECTORY)) return nullptr;

        if(strcmp(".", name) == 0) return node;
        if(strcmp("..", name) == 0){
            if(node->inode == 0) return fs::GetRoot();
            else return &nodes[tarNode->parentInode];
        }

        for(int i = 0; i < tarNode->entryCount; i++){
            if(strcmp(nodes[tarNode->children[i]].name, name) == 0) return &nodes[tarNode->children[i]];
        }

        return nullptr;
    }
}
