#include <tar.h>

#include <logging.h>

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
    long round = (sz % 512) ? (512 - (sz % 512)) : 0; // Round up to 512 byte multiple
    return (sz + round) / 512;
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
    size_t Read(struct fs_node* node, size_t offset, size_t size, uint8_t *buffer){
        if(node->vol){
            return ((TarVolume*)node->vol)->Read(node, offset, size, buffer);
        } else return 0;
    }

    size_t Write(struct fs_node* node, size_t offset, size_t size, uint8_t *buffer){
        if(node->vol){
            return ((TarVolume*)node->vol)->Write(node, offset, size, buffer);
        } else return 0;
    }

    void Open(struct fs_node* node, uint32_t flags){
        if(node->vol){
            ((TarVolume*)node->vol)->Open(node, flags);
        }
    }

    void Close(struct fs_node* node){
        if(node->vol){
            ((TarVolume*)node->vol)->Close(node);
        }
    }
    int ReadDir(struct fs_node* node, struct fs_dirent* dirent, uint32_t index){
        if(node->vol){
            return ((TarVolume*)node->vol)->ReadDir(node, dirent, index);
        } else return -10;
    }
    fs_node* FindDir(struct fs_node* node, char* name){
        if(node->vol){
            return ((TarVolume*)node->vol)->FindDir(node, name);
        } else return nullptr;
    }

    fs_node_t nodeTemplate = {
        .read = tar::Read,
        .write = tar::Write,
        .open = tar::Open,
        .close = tar::Close,
        .readDir = tar::ReadDir,
        .findDir = tar::FindDir,
    };

    void TarVolume::MakeNode(tar_header_t* header, tar_node_t* n, ino_t inode, ino_t parent, tar_header_t* dirHeader){
        n->parent = parent;
        n->header = header;

        n->node = nodeTemplate;
        n->node.inode = inode;
        n->node.uid = OctToDec(header->ustar.uid, 8);
        n->node.flags = TarTypeToFilesystemFlags(header->ustar.type);
        n->node.vol = this;

        char* name = header->ustar.name;
        char* _name = strtok(header->ustar.name, "/");
        while(_name){
            name = _name;
            _name = strtok(NULL, "/");
        }
        strcpy(n->node.name, name);
        n->node.size = GetSize(header->ustar.size);
    }

    int TarVolume::ReadDirectory(int blockIndex, ino_t parent){
        ino_t dirInode = nextNode++;
        tar_header_t* dirHeader = &blocks[blockIndex];
        tar_node_t* dirNode = &nodes[dirInode];
        MakeNode(dirHeader, dirNode, dirInode, parent);
        dirNode->entryCount = 0;

        int i = blockIndex + GetBlockCount(dirHeader->ustar.size) + 1; // Next block
        for(; i < blockCount; dirNode->entryCount++){
            if(strncmp(blocks[i].ustar.name, dirHeader->ustar.name, strlen(dirHeader->ustar.name))){
                break; // End of directory - header is not in directory
            }
            i += GetBlockCount(blocks[i].ustar.size) + 1;
        }
        
        dirNode->children = (ino_t*)kmalloc(sizeof(ino_t) * dirNode->entryCount);

        i = blockIndex + GetBlockCount(dirHeader->ustar.size) + 1;
        for(int e = 0; i < blockCount && e < dirNode->entryCount; e++){ // Iterate through directory
            ino_t inode = nextNode++;
            tar_node_t* n = &nodes[inode];
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

        Log::Info(" [TAR] Base: %x, Size: %x", base, size);

        int entryCount = 0;
        for(uint64_t i = 0; i < blockCount; i++, nodeCount++){ // Get file count
            tar_header_t header = blocks[i];

            if(!strchr(header.ustar.name, '/') || (header.ustar.type == TAR_TYPE_DIRECTORY && strchr(header.ustar.name, '/') == header.ustar.name + strlen(header.ustar.name) - 1)) entryCount++;

            i += GetBlockCount(header.ustar.size);
        }

        nodes = (tar_node_t*)kmalloc(nodeCount * sizeof(tar_node_t));
        memset(nodes, 0, nodeCount * sizeof(tar_node_t));

        tar_node_t* volumeNode = &nodes[0];
        nodes[0] = (tar_node_t){
            .header = nullptr,
            .node = {
                .flags = FS_NODE_DIRECTORY | FS_NODE_MOUNTPOINT,
                .inode = 0,
                .size = size,
                .read = fs::tar::Read,
                .write = fs::tar::Write,
                .open = fs::tar::Open,
                .close = fs::tar::Close,
                .readDir = fs::tar::ReadDir,
                .findDir = fs::tar::FindDir,
                .vol = this,
            },
            .parent = 0,
        };
        strcpy(nodes[0].node.name,name);
        mountPoint = volumeNode->node;
        strcpy(mountPoint.name, volumeNode->node.name);
        strcpy(mountPointDirent.name, mountPoint.name);

        volumeNode->children = (ino_t*)kmalloc(sizeof(ino_t) * entryCount);
        volumeNode->entryCount = entryCount;

        for(int i = 0, e = 0; i < blockCount; e++){
            tar_header_t header = blocks[i];

            if(header.ustar.type == TAR_TYPE_DIRECTORY){
                volumeNode->children[e] = nextNode; // Directory will take next available node so add it to children
                i = ReadDirectory(i, 0);
                continue;
            } else {
                ino_t inode = nextNode++;
                tar_node_t* node = &nodes[inode];
                MakeNode(&blocks[i], node, inode, 0);
                volumeNode->children[e] = inode;
            }

            i += GetBlockCount(header.ustar.size);
            i++;
        }
    }

    size_t TarVolume::Read(struct fs_node* node, size_t offset, size_t size, uint8_t *buffer){
        tar_node_t* tarNode = &nodes[node->inode];

		if(offset > node->size) return 0;
		else if(offset + size > node->size || size > node->size) size = node->size - offset;

		if(!size) return 0;

		memcpy(buffer, (void*)(((uintptr_t)tarNode->header) + 512 + offset), size);
		return size;
    }

    size_t TarVolume::Write(struct fs_node* node, size_t offset, size_t size, uint8_t *buffer){
        tar_node_t* tarNode = &nodes[node->inode];

        return 0;
    }

    void TarVolume::Open(struct fs_node* node, uint32_t flags){
        tar_node_t* tarNode = &nodes[node->inode];
    }

    void TarVolume::Close(struct fs_node* node){
        tar_node_t* tarNode = &nodes[node->inode];
    }

    int TarVolume::ReadDir(struct fs_node* node, struct fs_dirent* dirent, uint32_t index){
        tar_node_t* tarNode = &nodes[node->inode];
        
        if(!(node->flags & FS_NODE_DIRECTORY)) return -1;

        if(index >= tarNode->entryCount + 2) return -2;

        if(index == 0){
            strcpy(dirent->name, ".");
            return 0;
        } else if(index == 1){
            strcpy(dirent->name, "..");
            return 0;
        }

        tar_node_t* dir = &nodes[tarNode->children[index - 2]];

        Log::Error(dir->header->ustar.name);

        strcpy(dirent->name, dir->node.name);
        dirent->type = dir->node.flags;
        dirent->inode = dir->node.inode;

        return 0;
    }

    fs_node* TarVolume::FindDir(struct fs_node* node, char* name){
        tar_node_t* tarNode = &nodes[node->inode];
        
        if(!(node->flags & FS_NODE_DIRECTORY)) return nullptr;

        if(strcmp(".", name) == 0) return node;
        if(strcmp("..", name) == 0){
            if(node->inode == 0) return fs::GetRoot();
            else return &nodes[tarNode->parent].node;
        }

        for(int i = 0; i < tarNode->entryCount; i++){
            if(strcmp(nodes[tarNode->children[i]].node.name, name) == 0) return &nodes[tarNode->children[i]].node;
        }

        return nullptr;
    }
}