#include <Fs/Tmp.h>

#include <Errno.h>
#include <Debug.h>

namespace fs::Temp{
    TempVolume::TempVolume(const char* name){
        mountPoint = new TempNode(this, FileType::Directory);
        //mountPoint->parent = fs::root();

        mountPointDirent = DirectoryEntry(mountPoint, name);
    }

    void TempVolume::SetVolumeID(volume_id_t id){
        volumeID = id;

        mountPoint->volumeID = volumeID;
    }

    void TempVolume::ReallocateNode(TempNode* node, size_t oldSize){
        unsigned newBufferSize = (node->size + (TEMP_BUFFER_CHUNK_SIZE - 1)) & (~(TEMP_BUFFER_CHUNK_SIZE - 1)); // Round up to multiple of the chunk size

        if(newBufferSize <= 0){
            if(node->buffer){
                delete node->buffer;

                node->buffer = nullptr;
            }

            memoryUsage -= node->bufferSize;
            node->bufferSize = 0;
        } else if(newBufferSize > node->bufferSize){
            if(!node->buffer){
                node->buffer = new uint8_t[newBufferSize];

                memoryUsage += newBufferSize;
            } else {
                uint8_t* newBuffer = new uint8_t[newBufferSize];
                memcpy(newBuffer, node->buffer, oldSize);

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

    TempNode::TempNode(TempVolume* v, FileType ft){
        vol = v;
        volumeID = v->volumeID;

        handleCount = 0;
        size = 0;

        nlink = 1;

        type = ft;

        if(IsFile()){
            bufferLock = ReadWriteLock();
            buffer = nullptr;
            bufferSize = 0;
        } else if(IsDirectory()){
            children = List<DirectoryEntry>();
        } else {
            assert(!"TempNode not regular file or directory!");
        }
    }

    TempNode::~TempNode(){
        if(IsDirectory()){ // Check if we are a directory
            for(auto& ent : children){
                Unlink(&ent, true); // Unlink all files
            }
        } else if(buffer){
            delete buffer;
        }
    }

    TempNode* TempNode::Find(const char* name){
        assert(IsDirectory());

        for(DirectoryEntry& ent : children){
            if(strncmp(ent.name, name, NAME_MAX) == 0){
                return reinterpret_cast<TempNode*>(ent.node);
            }
        }

        return nullptr;
    }

    ErrorOr<ssize_t> TempNode::Read(size_t off, size_t readSize, UIOBuffer* readBuffer){
        if(IsDirectory()){
            return Error{EISDIR};
        }

        if(off > size){
            return 0;
        }

        if(off + readSize > size){
            readSize = size - off;
        }

        bufferLock.AcquireRead();
        if(readBuffer->Write(buffer + off, readSize)) {
            bufferLock.ReleaseRead();
            return EFAULT;
        }
        bufferLock.ReleaseRead();

        return readSize;
    }

    ErrorOr<ssize_t> TempNode::Write(size_t off, size_t writeSize, UIOBuffer* writeBuffer){
        if(IsDirectory()){
            return Error{EISDIR};
        }

        if(writeSize == 0) {
            return 0;
        }

        bufferLock.AcquireWrite();
        if(!buffer || off + writeSize > size){
            size_t old = size;
            size = off + writeSize;

            if(size >= bufferSize){
                vol->ReallocateNode(this, old);
            }
        }

        Log::Debug(debugLevelTmpFS, DebugLevelVerbose, "Writing (offset: %u, size: %u, nsize: %u, bsize: %u)", off, writeSize, size, bufferSize);

        if(writeBuffer->Read(buffer + off, writeSize)) {
            bufferLock.ReleaseWrite();
            return EFAULT;
        }
        bufferLock.ReleaseWrite();

        return writeSize;
    }

    Error TempNode::Truncate(off_t length){
        if(length < 0){
            return Error{EINVAL}; // No negative length
        }

        if(IsDirectory()){
            return Error{EISDIR}; // Cannot truncate directories
        }

        bufferLock.AcquireWrite();

        size_t old = size;
        size = length;
        vol->ReallocateNode(this, old);

        bufferLock.ReleaseWrite();

        return ERROR_NONE;
    }

    ErrorOr<int> TempNode::ReadDir(DirectoryEntry* dirent, uint32_t index){
        if(!IsDirectory()) {
            return Error{ENOTDIR};
        }

        if(index >= children.get_length() + 2){
            return 0; // Out of range
        }

        if(index == 0){
            strcpy(dirent->name, ".");
            dirent->flags = DirectoryEntry::FileToDirentFlags(type);
            
            return 1;
        } else if(index == 1){
            strcpy(dirent->name, "..");
            if(!parent){
                dirent->flags = DirectoryEntry::FileToDirentFlags(fs::root()->type);
            } else {
                dirent->flags = DirectoryEntry::FileToDirentFlags(parent->type);
            }

            return 1;
        } else {
            *dirent = children[index - 2];
            dirent->flags = DirectoryEntry::FileToDirentFlags(dirent->node->type);
            dirent->node = nullptr; // Do not expose node

            return 1;
        }
    }

    ErrorOr<FsNode*> TempNode::FindDir(const char* name){
        if(strcmp(name, ".") == 0){
            return this;
        }

        if(strcmp(name, "..") == 0){
            if(this == reinterpret_cast<TempNode*>(&vol->mountPoint)){
                return parent;
            } else {
                return this;
            }
        }

        for(DirectoryEntry& ent : children){
            if(strncmp(ent.name, name, NAME_MAX) == 0){
                IF_DEBUG(debugLevelTmpFS >= DebugLevelVerbose, {
                    Log::Info("found node: %x (%s)", ent.node, ent.name);
                });
                return ent.node;
            }
            IF_DEBUG(debugLevelTmpFS >= DebugLevelVerbose, {
                Log::Info("searched node: %x (%s)", ent.node, ent.name);
            });
        }

        return Error{ENOENT};
    }

    Error TempNode::Create(DirectoryEntry* ent, uint32_t mode){
        if(!IsDirectory()){
            IF_DEBUG(debugLevelTmpFS >= DebugLevelNormal, {
                Log::Warning("[tmpfs] Create: Node is not a directory!");
            });

            return Error{ENOTDIR};
        }

        if(Find(ent->name)){
            return Error{EEXIST};
        }

        TempNode* newNode = new TempNode(vol, FileType::Regular);
        newNode->parent = this;
        
        *ent = DirectoryEntry(newNode, ent->name);
        DirectoryEntry dirent = *ent;

        IF_DEBUG(debugLevelTmpFS >= DebugLevelNormal, {
            Log::Info("[tmpfs] created %s (addr: %x, nlink: %d)!", dirent.name, newNode, newNode->nlink);
        });

        children.add_back(dirent);

        return ERROR_NONE;
    }

    Error TempNode::CreateDirectory(DirectoryEntry* ent, uint32_t mode){
        if(!IsDirectory()){
            IF_DEBUG(debugLevelTmpFS >= DebugLevelNormal, {
                Log::Warning("[tmpfs] CreateDirectory: Node is not a directory!");
            });

            return Error{ENOTDIR};
        }

        if(Find(ent->name)){
            return Error{EEXIST};
        }

        TempNode* newNode = new TempNode(vol, FileType::Directory);
        newNode->parent = this;
        
        *ent = DirectoryEntry(newNode, ent->name);
        DirectoryEntry dirent = *ent;

        children.add_back(dirent);

        return ERROR_NONE;
    }
        
    Error TempNode::Link(FsNode* file, DirectoryEntry* ent){
        if(!IsDirectory()){
            return Error{ENOTDIR};
        }

        if(file->volumeID != vol->volumeID){
            return Error{EXDEV}; // Different filesystem
        }
        
        if(Find(ent->name)){
            return Error{EEXIST};
        }

        if(file->IsDirectory()){
            IF_DEBUG(debugLevelTmpFS >= DebugLevelNormal, {
                Log::Warning("[tmpfs] Link: File to hard link is a directory!");
            });
            return Error{EPERM}; // Don't hard link directories
        }

        DirectoryEntry dirent(file, ent->name);

        file->nlink++;

        children.add_back(dirent);

        *ent = dirent;

        return ERROR_NONE;
    }

    Error TempNode::Unlink(DirectoryEntry* ent, bool unlinkDirectories){
        if(!IsDirectory()){
            IF_DEBUG(debugLevelTmpFS >= DebugLevelNormal, {
                Log::Warning("[tmpfs] Unlink: Node not a directory!");
            });
            return Error{ENOTDIR};
        }

        TempNode* node = Find(ent->name);
        if(!node){
            IF_DEBUG(debugLevelTmpFS >= DebugLevelNormal, {
                Log::Warning("[tmpfs] Unlink: '%s' does not exist!", ent->name);
            });
            return Error{ENOENT}; // Could not find entry with the name in ent
        }

        if(!unlinkDirectories && node->IsDirectory()){
            IF_DEBUG(debugLevelTmpFS >= DebugLevelNormal, {
                Log::Warning("[tmpfs] Unlink: Entry is a directory!");
            });
            return Error{EISDIR}; // Don't unlink directories unless explicitly told to do so
        }

        node->nlink--;

        for(auto it = children.begin(); it != children.end(); it++){
            if(!strcmp(ent->name, it->name)){
                children.remove(it);
                break;
            }
        }

        if(node->nlink <= 0 && node->handleCount <= 0){ // Check if there are any open handles to the node
            Log::Debug(debugLevelTmpFS, DebugLevelVerbose, "[tmpfs] Deleting node (%s)!", ent->name);
            delete node;
        }

        return ERROR_NONE;
    }

    ErrorOr<File*> TempNode::Open(size_t flags){
        return TRY_OR_ERROR(NodeFile::Create(this));
    }

    void TempNode::Close(){
        handleCount--;

        if(handleCount <= 0 && nlink <= 0){ // No hard links or open handles to node
            Log::Debug(debugLevelTmpFS, DebugLevelNormal, "[tmpfs] Deleting node!");
            delete this;
        }
    }
}