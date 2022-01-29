#include <Fs/Filesystem.h>

#include <Errno.h>
#include <Fs/FsVolume.h>
#include <Fs/VolumeManager.h>
#include <Logging.h>
#include <Panic.h>
#include <Scheduler.h>

#include <Debug.h>

void FilesystemBlocker::Interrupt() {
    shouldBlock = false;
    interrupted = true;

retry:
    acquireLock(&lock);
    if (node && !removed) {
        if (acquireTestLock(&node->blockedLock)) {
            releaseLock(&lock);

            Scheduler::Yield();
            goto retry;
        }

        node->blocked.remove(this);
        removed = true;

        releaseLock(&node->blockedLock);
    }

    if (thread) {
        thread->Unblock();
    }
    releaseLock(&lock);
}

FilesystemBlocker::~FilesystemBlocker() {
retry:
    if (node && !removed) {
        if (acquireTestLock(&node->blockedLock)) {
            goto retry;
        }

        node->blocked.remove(this);
        releaseLock(&node->blockedLock);
    }
}

DirectoryEntry::DirectoryEntry(FsNode* node, const char* name) : node(node) {
    strncpy(this->name, name, NAME_MAX);

    switch (node->flags & FS_NODE_TYPE) {
    case FS_NODE_FILE:
        flags = DT_REG;
        break;
    case FS_NODE_DIRECTORY:
        flags = DT_DIR;
        break;
    case FS_NODE_CHARDEVICE:
        flags = DT_CHR;
        break;
    case FS_NODE_BLKDEVICE:
        flags = DT_BLK;
        break;
    case FS_NODE_SOCKET:
        flags = DT_SOCK;
        break;
    case FS_NODE_SYMLINK:
        flags = DT_LNK;
        break;
    }
}

namespace fs {
volume_id_t nextVID = 1; // Next volume ID

List<FsDriver*> drivers;

void RegisterDriver(FsDriver* driver) {
    for (auto& drv : drivers) {
        assert(drv != driver);
        assert(strcmp(drv->ID(), driver->ID()));
    }

    drivers.add_back(driver);
}

void UnregisterDriver(FsDriver* driver) {
    for (auto it = drivers.begin(); it != drivers.end(); it++) {
        if ((*it) == driver) {
            drivers.remove(it);
            return;
        }
    }

    assert(!"Driver not found!");
}

FsDriver* IdentifyFilesystem(FsNode* node) {
    for (auto drv : drivers) {
        if (drv->Identify(node)) {
            return drv;
        }
    }

    return nullptr; // No driver found
}

class Root : public FsNode {
public:
    Root() {
        inode = 0;
        flags = FS_NODE_DIRECTORY;
    }

    int ReadDir(DirectoryEntry*, uint32_t);
    FsNode* FindDir(const char* name);

    int Create(DirectoryEntry* ent, uint32_t mode) {
        Log::Warning("[RootFS] Attempted to create a file!");
        return -EROFS;
    }

    int CreateDirectory(DirectoryEntry* ent, uint32_t mode) { return -EROFS; }
};

Root root;
DirectoryEntry rootDirent = DirectoryEntry(&root, "");

DirectoryEntry* devices[64];
uint32_t deviceCount = 0;

FsNode* GetRoot() { return &root; }

FsNode* FollowLink(FsNode* link, FsNode* workingDir) {
    assert(link);

    char buffer[PATH_MAX + 1];

    auto bytesRead = link->ReadLink(buffer, PATH_MAX);
    if (bytesRead < 0) {
        Log::Warning("FollowLink: Readlink error %d", -bytesRead);
        return nullptr;
    }
    buffer[bytesRead] = 0; // Null terminate

    FsNode* node = ResolvePath(buffer, workingDir, false);

    if (!node) {
        Log::Warning("FollowLink: Failed to resolve symlink %s!", buffer);
    }
    return node;
}

FsNode* ResolvePath(const String& path, const char* workingDir, bool followSymlinks) {
    if(path.Empty()){
        Log::Warning("fs::ResolvePath: path is empty!");
        return nullptr;
    }

    if(!path.Compare("/")){
        return fs::GetRoot();
    }

    Log::Debug(debugLevelFilesystem, DebugLevelVerbose, "Opening '%s'", path.c_str());

    assert(path.Length() >= 1);

    if (workingDir && path[0] != '/') { // If the path starts with '/' then treat as an absolute path
        FsNode* wdNode = ResolvePath(workingDir, (FsNode*)nullptr);
        assert(wdNode);
        return ResolvePath(path, wdNode, followSymlinks);
    } else {
        return ResolvePath(path, (FsNode*)nullptr, followSymlinks);
    }
}

FsNode* ResolvePath(const String& path, FsNode* workingDir, bool followSymlinks) {
    if(path.Empty()){
        Log::Warning("fs::ResolvePath: path is empty!");
        return nullptr;
    }

    FsNode* root = fs::GetRoot();
    FsNode* currentNode = root;

    if (workingDir && path[0] != '/') {
        currentNode = workingDir;
    }

    Vector<String> components = path.Split('/');
    if(components.size() == 0){
        if(path.Compare("/") == 0){
            return fs::GetRoot();
        } else {
            return nullptr;
        }
    }
    
    unsigned i = 0;
    for(; i < components.size() - 1; i++){
        String& component = components[i];
        FsNode* node = fs::FindDir(currentNode, component.c_str());
        if (!node) {
            Log::Debug(debugLevelFilesystem, DebugLevelVerbose, "%s not found!", component.c_str());

            return nullptr;
        }

        size_t amountOfSymlinks = 0;
        if(node->IsSymlink()) { // Check for symlinks
            if (amountOfSymlinks++ > MAXIMUM_SYMLINK_AMOUNT) {
                IF_DEBUG((debugLevelFilesystem >= DebugLevelNormal),
                         { Log::Warning("ResolvePath: Reached maximum number of symlinks"); });
                return nullptr;
            }

            node = FollowLink(node, currentNode);

            if (!node) {
                IF_DEBUG((debugLevelFilesystem >= DebugLevelNormal),
                         { Log::Warning("ResolvePath: Unresolved symlink!"); });
                return node;
            }
        }

        if(!(node->IsDirectory())){
            Log::Debug(debugLevelFilesystem, DebugLevelNormal, "Failed to resolve path component: Expected a directory at '%s'!", component.c_str());
            return nullptr;
        }

        currentNode = node;
    }

    if(i >= components.size()){
        return currentNode;
    }

    FsNode* node = fs::FindDir(currentNode, components[i].c_str());
    if(!node){
        Log::Debug(debugLevelFilesystem, DebugLevelVerbose, "ResolvePath: Failed to find %s!", components[i].c_str());
        return nullptr;
    }

    size_t amountOfSymlinks = 0;
    while(followSymlinks && node->IsSymlink()) { // Check for symlinks
        if (amountOfSymlinks++ > MAXIMUM_SYMLINK_AMOUNT) {
            IF_DEBUG((debugLevelFilesystem >= DebugLevelVerbose),
                        { Log::Warning("ResolvePath: Reached maximum number of symlinks"); });
            return nullptr;
        }

        node = FollowLink(node, currentNode);

        if (!node) {
            IF_DEBUG((debugLevelFilesystem >= DebugLevelNormal),
                        { Log::Warning("ResolvePath: Unresolved symlink!"); });
            return nullptr;
        }
    }

    Log::Debug(debugLevelFilesystem, DebugLevelVerbose, "Found %s!", components[i].c_str());
    return node;
}

FsNode* ResolveParent(const char* path, const char* workingDir) {
    char pathCopy[strlen(path) + 1];
    strcpy(pathCopy, path);

    if (pathCopy[strlen(pathCopy) - 1] == '/') { // Remove trailing slash
        pathCopy[strlen(pathCopy) - 1] = 0;
    }

    char* dirPath = strrchr(pathCopy, '/');

    FsNode* parentDirectory = nullptr;

    if (dirPath == nullptr) {
        parentDirectory = fs::ResolvePath(workingDir);
    } else {
        *(dirPath - 1) = 0;      // Cut off the directory name from the path copy
        if (!strlen(pathCopy)) { // Root
            return fs::GetRoot();
        } else {
            parentDirectory = fs::ResolvePath(pathCopy, workingDir);
        }
    }

    return parentDirectory;
}

FsNode* ResolveParent(const char* path, FsNode* workingDir) {
    char* pathCopy = (char*)kmalloc(strlen(path) + 1);
    strcpy(pathCopy, path);

    if (pathCopy[strlen(pathCopy) - 1] == '/') { // Remove trailing slash
        pathCopy[strlen(pathCopy) - 1] = 0;
    }

    char* dirPath = strrchr(pathCopy, '/');

    FsNode* parentDirectory = nullptr;

    if (dirPath == nullptr) {
        parentDirectory = workingDir;
    } else {
        *(dirPath - 1) = 0; // Cut off the directory name from the path copy
        parentDirectory = fs::ResolvePath(pathCopy, workingDir);
    }

    kfree(pathCopy);
    return parentDirectory;
}

char* CanonicalizePath(const char* path, char* workingDir) {
    char* tempPath;
    if (workingDir && path[0] != '/') {
        tempPath = (char*)kmalloc(strlen(path) + strlen(workingDir) + 2);
        strcpy(tempPath, workingDir);
        strcpy(tempPath + strlen(tempPath), "/");
        strcpy(tempPath + strlen(tempPath), path);
    } else {
        tempPath = (char*)kmalloc(strlen(path) + 1);
        strcpy(tempPath, path);
    }

    char* savePtr = nullptr;
    char* file = strtok_r(tempPath, "/", &savePtr);
    List<char*>* tokens = new List<char*>();

    while (file != NULL) {
        tokens->add_back(file);
        file = strtok_r(NULL, "/", &savePtr);
    }

    int newLength = 2; // Separator and null terminator
    newLength += strlen(path) + strlen(workingDir);
    for (unsigned i = 0; i < tokens->get_length(); i++) {
        if (strlen(tokens->get_at(i)) == 0) {
            tokens->remove_at(i--);
            continue;
        } else if (strcmp(tokens->get_at(i), ".") == 0) {
            tokens->remove_at(i--);
            continue;
        } else if (strcmp(tokens->get_at(i), "..") == 0) {
            if (i) {
                tokens->remove_at(i);
                tokens->remove_at(i - 1);
                i -= 2;
            } else
                tokens->remove_at(i);
            continue;
        }

        newLength += strlen(tokens->get_at(i)) + 1; // Name and separator
    }

    char* outPath = (char*)kmalloc(newLength);
    outPath[0] = 0;

    if (!tokens->get_length())
        strcpy(outPath + strlen(outPath), "/");
    else
        for (unsigned i = 0; i < tokens->get_length(); i++) {
            strcpy(outPath + strlen(outPath), "/");
            strcpy(outPath + strlen(outPath), tokens->get_at(i));
        }

    kfree(tempPath);
    delete tokens;

    return outPath;
}

char* BaseName(const char* path) {
    char* pathCopy = (char*)kmalloc(strlen(path) + 1);
    strcpy(pathCopy, path);

    if (pathCopy[strlen(pathCopy) - 1] == '/') { // Remove trailing slash
        pathCopy[strlen(pathCopy) - 1] = 0;
    }

    char* basename = nullptr;
    char* temp;
    if ((temp = strrchr(pathCopy, '/'))) {
        basename = (char*)kmalloc(strlen(temp) + 1);
        strcpy(basename, temp);

        kfree(pathCopy);
    } else {
        basename = pathCopy;
    }

    return basename;
}

int Root::ReadDir(DirectoryEntry* dirent, uint32_t index) {
    if(index == 0){
        *dirent = DirectoryEntry(this, ".");
        return 1;
    } else if(index == 1){
        *dirent = DirectoryEntry(this, "..");
        return 1;
    }

    if (index - 2 < VolumeManager::volumes->get_length()) {
        *dirent = (VolumeManager::volumes->get_at(index - 2)->mountPointDirent);
        return 1;
    } else {
        return 0;
    }
}

FsNode* Root::FindDir(const char* name) {
    if (strcmp(name, ".") == 0)
        return this;
    if (strcmp(name, "..") == 0)
        return this;

    for (unsigned i = 0; i < VolumeManager::volumes->get_length(); i++) {
        if (strcmp(VolumeManager::volumes->get_at(i)->mountPointDirent.name, name) == 0)
            return (VolumeManager::volumes->get_at(i)->mountPointDirent.node);
    }

    return nullptr;
}

ssize_t Read(FsNode* node, size_t offset, size_t size, void* buffer) {
    assert(node);

    return node->Read(offset, size, reinterpret_cast<uint8_t*>(buffer));
}

ssize_t Write(FsNode* node, size_t offset, size_t size, void* buffer) {
    assert(node);

    return node->Write(offset, size, reinterpret_cast<uint8_t*>(buffer));
}

fs_fd_t* Open(FsNode* node, uint32_t flags) { return node->Open(flags); }

int Link(FsNode* dir, FsNode* link, DirectoryEntry* ent) {
    assert(dir);
    assert(link);

    return dir->Link(link, ent);
}

int Unlink(FsNode* dir, DirectoryEntry* ent, bool unlinkDirectories) {
    assert(dir);
    assert(ent);

    return dir->Unlink(ent, unlinkDirectories);
}

void Close(FsNode* node) { return node->Close(); }

void Close(fs_fd_t* fd) {
    if (!fd)
        return;

    ScopedSpinLock lockOpenFileData(fd->dataLock);

    assert(fd->node);

    fd->node->Close();
    fd->node = nullptr;
}

int ReadDir(FsNode* node, DirectoryEntry* dirent, uint32_t index) {
    assert(node);

    return node->ReadDir(dirent, index);
}

FsNode* FindDir(FsNode* node, const char* name) {
    assert(node);

    return node->FindDir(name);
}

ssize_t Read(const FancyRefPtr<UNIXOpenFile>& handle, size_t size, uint8_t* buffer) {
    assert(handle->node);
    ssize_t ret = Read(handle->node, handle->pos, size, buffer);

    if (ret > 0) {
        handle->pos += ret;
    }

    return ret;
}

ssize_t Write(const FancyRefPtr<UNIXOpenFile>& handle, size_t size, uint8_t* buffer) {
    assert(handle->node);
    off_t ret = Write(handle->node, handle->pos, size, buffer);

    if (ret >= 0) {
        handle->pos += ret;
    }

    return ret;
}

int ReadDir(const FancyRefPtr<UNIXOpenFile>& handle, DirectoryEntry* dirent, uint32_t index) {
    assert(handle->node);

    return ReadDir(handle->node, dirent, index);
}

FsNode* FindDir(const FancyRefPtr<UNIXOpenFile>& handle, const char* name) {
    assert(handle->node);

    return FindDir(handle->node, name);
}

int Ioctl(const FancyRefPtr<UNIXOpenFile>& handle, uint64_t cmd, uint64_t arg) {
    assert(handle->node);

    return handle->node->Ioctl(cmd, arg);
}

int Rename(FsNode* olddir, char* oldpath, FsNode* newdir, char* newpath) {
    assert(olddir && newdir);

    if ((olddir->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        return -ENOTDIR;
    }

    if ((newdir->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        return -ENOTDIR;
    }

    FsNode* oldnode = ResolvePath(oldpath, olddir);

    if (!oldnode) {
        return -ENOENT;
    }

    if ((oldnode->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY) {
        Log::Warning("Filesystem: Rename: We do not support using rename on directories yet!");
        return -ENOSYS;
    }

    FsNode* newpathParent = ResolveParent(newpath, newdir);

    if (!newpathParent) {
        return -ENOENT;
    } else if ((newpathParent->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        return -ENOTDIR; // Parent of newpath is not a directory
    }

    FsNode* newnode = ResolvePath(newpath, newdir);

    if (newnode) {
        if ((newnode->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY) {
            return -EISDIR; // If it exists newpath must not be a directory
        }
    }

    DirectoryEntry oldpathDirent;
    strncpy(oldpathDirent.name, fs::BaseName(oldpath), NAME_MAX);

    DirectoryEntry newpathDirent;
    strncpy(newpathDirent.name, fs::BaseName(newpath), NAME_MAX);

    if ((oldnode->flags & FS_NODE_TYPE) != FS_NODE_SYMLINK &&
        oldnode->volumeID == newpathParent->volumeID) { // Easy shit we can just link and unlink
        FsNode* oldpathParent = fs::ResolveParent(oldpath, olddir);
        assert(oldpathParent); // If this is null something went horribly wrong

        if (newnode) {
            if (auto e = newpathParent->Unlink(&newpathDirent)) {
                return e; // Unlink error
            }
        }

        if (auto e = newpathParent->Link(oldnode, &newpathDirent)) {
            return e; // Link error
        }

        if (auto e = oldpathParent->Unlink(&oldpathDirent)) {
            return e; // Unlink error
        }
    } else if ((oldnode->flags & FS_NODE_TYPE) != FS_NODE_SYMLINK) { // Aight we have to copy it
        FsNode* oldpathParent = fs::ResolveParent(oldpath, olddir);
        assert(oldpathParent); // If this is null something went horribly wrong

        if (auto e = newpathParent->Create(&newpathDirent, 0)) {
            return e; // Create error
        }

        newnode = ResolvePath(newpath, newdir);
        if (!newnode) {
            Log::Warning(
                "Filesystem: Rename: newpath was created with no error returned however it was unable to be found.");
            return -ENOENT;
        }

        uint8_t* buffer = (uint8_t*)kmalloc(oldnode->size);

        ssize_t rret = oldnode->Read(0, oldnode->size, buffer);
        if (rret < 0) {
            Log::Warning("Filesystem: Rename: Error reading oldpath");
            return rret;
        }

        ssize_t wret = oldnode->Write(0, rret, buffer);
        if (wret < 0) {
            Log::Warning("Filesystem: Rename: Error reading oldpath");
            return wret;
        }

        if (auto e = oldpathParent->Unlink(&oldpathDirent)) {
            return e; // Unlink error
        }
    } else {
        Log::Warning("Filesystem: Rename: We do not support using rename on symlinks yet!"); // TODO: Rename the symlink
        return -ENOSYS;
    }

    return 0;
}
} // namespace fs