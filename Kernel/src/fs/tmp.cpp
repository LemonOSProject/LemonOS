#include <fs/tmp.h>

#include <errno.h>

namespace fs::Temp{
    TempVolume::TempVolume(const char* name){

        mountPoint = new TempNode(this, FS_NODE_DIRECTORY);

        mountPointDirent = DirectoryEntry(mountPoint, name);
    }

    void TempVolume::SetVolumeID(volume_id_t id){
        volumeID = id;

        mountPoint->volumeID = volumeID;
    }

    void TempVolume::ReallocateNode(TempNode* node){
        unsigned newBufferSize = (node->size + (TEMP_BUFFER_CHUNK_SIZE - 1)) & ~(TEMP_BUFFER_CHUNK_SIZE - 1); // Round up to multiple of the chunk size

        if(newBufferSize <= 0){
            if(node->buffer){
                delete node->buffer;

                node->buffer = nullptr;
            }

            node->bufferSize = 0;
        } else if(newBufferSize > node->bufferSize){
            if(!node->buffer){
                node->buffer = new uint8_t[node->size];

                memoryUsage += newBufferSize;
            } else {
                uint8_t* newBuffer = new uint8_t[newBufferSize];
                memcpy(newBuffer, node->buffer, node->bufferSize);

                delete node->buffer;
                node->buffer = newBuffer;

                memoryUsage += newBufferSize - node->bufferSize;
            }

            node->bufferSize = newBufferSize;
        } else if(newBufferSize < node->bufferSize){
            if(!node->buffer){
                node->buffer = new uint8_t[newBufferSize];

                memoryUsage += newBufferSize;
            } else {
                uint8_t* newBuffer = new uint8_t[newBufferSize];
                memcpy(newBuffer, node->buffer, node->size);

                delete node->buffer;
                node->buffer = newBuffer;

                memoryUsage -= (node->bufferSize - newBufferSize);
            }

            node->bufferSize = newBufferSize;
        }
    }

    TempNode::TempNode(TempVolume* v, int createFlags){
        vol = v;
        volumeID = v->volumeID;

        flags = createFlags;

        if((flags & FS_NODE_TYPE) == FS_NODE_FILE){
            bufferLock = ReadWriteLock();
            buffer = nullptr;
            bufferSize = 0;
        } else if((flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
            children = List<DirectoryEntry>();
        } else {
            assert(!"TempNode not regular file or directory!");
        }
    }

    TempNode::~TempNode(){
        if(buffer){
            delete buffer;
        }
    }

    TempNode* TempNode::Find(const char* name){
        assert((flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY);

        for(DirectoryEntry& ent : children){
            if(strncmp(ent.name, name, NAME_MAX) == 0){
                return reinterpret_cast<TempNode*>(ent.node);
            }
        }

        return nullptr;
    }

    ssize_t TempNode::Read(size_t off, size_t readSize, uint8_t* readBuffer){
        if((flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
            return -EISDIR;
        }

        if(off > size){
            return 0;
        }

        if(off + readSize > size){
            readSize = size - off;
        }

        bufferLock.AcquireRead();
        memcpy(readBuffer, buffer, readSize);
        bufferLock.ReleaseRead();

        return readSize;
    }

    ssize_t TempNode::Write(size_t off, size_t writeSize, uint8_t* writeBuffer){
        if((flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
            return -EISDIR;
        }

        bufferLock.AcquireWrite();
        if(off + writeSize > size){
            size = off + writeSize;

            vol->ReallocateNode(this);
        }

        memcpy(buffer + off, writeBuffer, writeSize);
        bufferLock.ReleaseWrite();

        return writeSize;
    }

    int TempNode::Truncate(off_t length){
        if(length < 0){
            return -EINVAL; // No negative length
        }

        if(IsDirectory()){
            return -EISDIR; // Cannot truncate directories
        }

        bufferLock.AcquireWrite();

        size = length;
        vol->ReallocateNode(this);

        bufferLock.ReleaseWrite();

        return 0;
    }

    int TempNode::ReadDir(DirectoryEntry* dirent, uint32_t index){
        if(index >= children.get_length() + 2){
            return -ENOENT; // Out of range
        }

        if(index == 0){
            strcpy(dirent->name, ".");
            dirent->flags = flags;
            
            return 0;
        } else if(index == 1){
            strcpy(dirent->name, "..");
            dirent->flags = parent->flags;

            return 0;
        } else {
            *dirent = children[index];
            dirent->flags = dirent->node->flags;

            return 0;
        }
    }

    FsNode* TempNode::FindDir(char* name){
        if(strcmp(name, ".") == 0){
            return this;
        }

        if(strcmp(name, "..") == 0){
            if(this == reinterpret_cast<TempNode*>(&vol->mountPoint)){
                return fs::GetRoot();
            } else {
                return this;
            }
        }

        return Find(name);
    }

    int TempNode::Create(DirectoryEntry* ent, uint32_t mode){
        if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
            return -ENOTDIR;
        }

        TempNode* newNode = new TempNode(vol, FS_NODE_FILE);
        
        DirectoryEntry dirent = *ent;
        dirent.node = newNode;

        children.add_back(dirent);

        return 0;
    }

    int TempNode::CreateDirectory(DirectoryEntry* ent, uint32_t mode){
        if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
            return -ENOTDIR;
        }

        TempNode* newNode = new TempNode(vol, FS_NODE_DIRECTORY);
        newNode->parent = this;
        
        DirectoryEntry dirent = *ent;
        dirent.node = newNode;

        children.add_back(dirent);

        return 0;
    }
        
    int TempNode::Link(FsNode* file, DirectoryEntry* ent){
        if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
            return -ENOTDIR;
        }

        if(file->volumeID != vol->volumeID){
            return -EXDEV; // Different filesystem
        }

        DirectoryEntry dirent = *ent;
        ent->node = file;

        children.add_back(dirent);

        return 0;
    }

    int TempNode::Unlink(DirectoryEntry* ent, bool unlinkDirectories){
        if((flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
            return -ENOTDIR;
        }

        TempNode* node = Find(ent->name);
        if(!node){
            return -ENOENT; // Could not find entry with the name in ent
        }

        if(!unlinkDirectories && node->IsDirectory()){
            return -EISDIR; // Don't unlink directories unless explicitly told to do so
        }

        node->nlink--;
        if(node->handleCount <= 0){ // Check if there are any open handles to the node
            delete node;
        }

        return 0;
    }

    void TempNode::Close(){
        handleCount--;

        if(handleCount <= 0 && nlink <= 0){ // No hard links or open handles to node
            delete this;
        }
    }
}