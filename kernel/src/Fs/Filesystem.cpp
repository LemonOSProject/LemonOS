#include <Fs/Filesystem.h>

#include <Errno.h>
#include <Fs/FsVolume.h>
#include <Fs/VolumeManager.h>
#include <Logging.h>
#include <Panic.h>
#include <Scheduler.h>

#include <Debug.h>

/*void FilesystemBlocker::Interrupt() {
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
}*/

DirectoryEntry::DirectoryEntry(FsNode* node, const char* name) : node(node) {
    strncpy(this->name, name, NAME_MAX);

    switch (node->type) {
    case FileType::Regular:
        flags = DT_REG;
        break;
    case FileType::Directory:
        flags = DT_DIR;
        break;
    case FileType::CharDevice:
        flags = DT_CHR;
        break;
    case FileType::BlockDevice:
        flags = DT_BLK;
        break;
    case FileType::Socket:
        flags = DT_SOCK;
        break;
    case FileType::SymbolicLink:
        flags = DT_LNK;
        break;
    case FileType::Pipe:
        flags = DT_FIFO;
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
        type = FileType::Directory;
        dirent = DirectoryEntry(this, ".");
    }

    ErrorOr<int> read_dir(DirectoryEntry*, uint32_t);
    ErrorOr<FsNode*> find_dir(const char* name);

    Error create(DirectoryEntry* ent, uint32_t mode) {
        Log::Warning("[RootFS] Attempted to create a file!");
        return Error{EROFS};
    }

    Error create_directory(DirectoryEntry* ent, uint32_t mode) { return Error{EROFS}; }

    DirectoryEntry dirent;
};

Root* rootNode;

DirectoryEntry* devices[64];
uint32_t deviceCount = 0;

void create_root_fs() {
    rootNode = new Root();
}

FsNode* root() { return rootNode; }

FsNode* FollowLink(FsNode* link, FsNode* workingDir) {
    assert(link);

    char buffer[PATH_MAX + 1];

    auto bytesRead = link->read_link(buffer, PATH_MAX);
    if (bytesRead.HasError()) {
        Log::Warning("FollowLink: Readlink error %d", bytesRead.Err());
        return nullptr;
    }
    buffer[bytesRead.Value()] = 0; // Null terminate

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
        return fs::root();
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

    if(!path.Compare("/")){
        return fs::root();
    }

    FsNode* root = fs::root();
    FsNode* currentNode = root;

    if (workingDir && path[0] != '/') {
        currentNode = workingDir;
    }

    Vector<String> components = path.Split('/');
    if(components.size() == 0){
        if(path.Compare("/") == 0){
            return fs::root();
        } else {
            return nullptr;
        }
    }
    
    unsigned i = 0;
    for(; i < components.size() - 1; i++){
        String& component = components[i];
        auto r = fs::find_dir(currentNode, component.c_str());
        if (r.HasError()) {
            Log::Debug(debugLevelFilesystem, DebugLevelVerbose, "%s not found!", component.c_str());

            return nullptr;
        }

        FsNode* node = r.Value();
        if(!node) {
            Log::Error("component %s has no node!", component.c_str());
            assert(node);
        }

        size_t amountOfSymlinks = 0;
        if(node->is_symlink()) { // Check for symlinks
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

        if(!(node->is_directory())){
            Log::Debug(debugLevelFilesystem, DebugLevelNormal, "Failed to resolve path component: Expected a directory at '%s'!", component.c_str());
            return nullptr;
        }

        currentNode = node;
    }

    if(i >= components.size()){
        return currentNode;
    }

    auto r = fs::find_dir(currentNode, components[i].c_str());
    if(r.HasError()){
        Log::Debug(debugLevelFilesystem, DebugLevelVerbose, "ResolvePath: Failed to find %s!", components[i].c_str());
        return nullptr;
    }
    FsNode* node = r.Value();

    size_t amountOfSymlinks = 0;
    while(followSymlinks && node->is_symlink()) { // Check for symlinks
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
            return fs::root();
        } else {
            parentDirectory = fs::ResolvePath(pathCopy, workingDir);
        }
    }

    return parentDirectory;
}

FsNode* ResolveParent(const String& path, FsNode* workingDir) {
    String pathCopy = path;

    if (pathCopy[pathCopy.Length() - 1] == '/') { // Remove trailing slash
        pathCopy.Erase(pathCopy.Length() - 1);
    }

    unsigned dirPath = pathCopy.find_last_of('/');
    FsNode* parentDirectory = nullptr;

    if (dirPath == STRING_POS_NONE) {
        parentDirectory = workingDir;
    } else {
        pathCopy.Resize(dirPath); // Cut off the child name and path separator from the path copy
        parentDirectory = fs::ResolvePath(pathCopy, workingDir);
    }

    return parentDirectory;
}

String CanonicalizePath(const char* path, char* workingDir) {
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
    }

    String outPath;
    if (!tokens->get_length())
        outPath = "/";
    else
        for (unsigned i = 0; i < tokens->get_length(); i++) {
            outPath += String("/") + tokens->get_at(i);
        }

    kfree(tempPath);
    delete tokens;

    return outPath;
}

String BaseName(const String& path) {
    String pathCopy = path;

    if (pathCopy[pathCopy.Length() - 1] == '/') { // Remove trailing slash
        pathCopy.Erase(pathCopy.Length() - 1);
    }

    String basename;
    char* temp;
    if ((temp = strrchr(pathCopy.c_str(), '/'))) {
        basename = String(temp);
    } else {
        basename = std::move(pathCopy);
    }

    return basename;
}

ErrorOr<int> Root::read_dir(DirectoryEntry* dirent, uint32_t index) {
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

ErrorOr<FsNode*> Root::find_dir(const char* name) {
    if (strcmp(name, ".") == 0)
        return this;
    if (strcmp(name, "..") == 0)
        return this;

    for (unsigned i = 0; i < VolumeManager::volumes->get_length(); i++) {
        if (strcmp(VolumeManager::volumes->get_at(i)->mountPointDirent.name, name) == 0)
            return (VolumeManager::volumes->get_at(i)->mountPointDirent.inode());
    }

    return Error{ENOENT};
}

ErrorOr<ssize_t> read(FsNode* node, size_t offset, size_t size, void* buffer) {
    assert(node);

    UIOBuffer uio(buffer);
    return node->read(offset, size, &uio);
}

ErrorOr<ssize_t> write(FsNode* node, size_t offset, size_t size, void* buffer) {
    assert(node);

    UIOBuffer uio(buffer);
    return node->write(offset, size, &uio);
}

ErrorOr<File*> Open(FsNode* node, uint32_t flags) { return node->Open(flags); }

Error link(FsNode* dir, FsNode* link, DirectoryEntry* ent) {
    assert(dir);
    assert(link);

    return dir->link(link, ent);
}

Error Unlink(FsNode* dir, DirectoryEntry* ent, bool unlinkDirectories) {
    assert(dir);
    assert(ent);

    return dir->unlink(ent, unlinkDirectories);
}

void Close(File* fd) {
    if (!fd)
        return;

    ScopedSpinLock lockOpenFileData(fd->fLock);

    assert(fd->inode);

    fd->inode->Close();
    fd->inode = nullptr;
}

ErrorOr<int> read_dir(FsNode* node, DirectoryEntry* dirent, uint32_t index) {
    assert(node);

    return node->read_dir(dirent, index);
}

ErrorOr<FsNode*> find_dir(FsNode* node, const char* name) {
    assert(node);

    return node->find_dir(name);
}

Error Rename(FsNode* olddir, const char* oldpath, FsNode* newdir, const char* newpath) {
    assert(olddir && newdir);

    if (!olddir->is_directory()) {
        return Error{ENOTDIR};
    }

    if (!newdir->is_directory()) {
        return Error{ENOTDIR};
    }

    FsNode* oldnode = ResolvePath(oldpath, olddir);

    if (!oldnode) {
        return Error{ENOENT};
    }

    if (oldnode->is_directory()) {
        Log::Warning("Filesystem: Rename: We do not support using rename on directories yet!");
        return Error{ENOSYS};
    }

    FsNode* newpathParent = ResolveParent(newpath, newdir);

    if (!newpathParent) {
        return Error{ENOENT};
    } else if (!newpathParent->is_directory()) {
        return Error{ENOTDIR}; // Parent of newpath is not a directory
    }

    FsNode* newnode = ResolvePath(newpath, newdir);

    if (newnode) {
        if (newnode->is_directory()) {
            return Error{EISDIR}; // If it exists newpath must not be a directory
        }
    }

    DirectoryEntry oldpathDirent;
    strncpy(oldpathDirent.name, fs::BaseName(oldpath).c_str(), NAME_MAX);

    DirectoryEntry newpathDirent;
    strncpy(newpathDirent.name, fs::BaseName(newpath).c_str(), NAME_MAX);

    if (!oldnode->is_symlink() &&
        oldnode->volumeID == newpathParent->volumeID) { // Easy shit we can just link and unlink
        FsNode* oldpathParent = fs::ResolveParent(oldpath, olddir);
        assert(oldpathParent); // If this is null something went horribly wrong

        if (newnode) {
            if (auto e = newpathParent->unlink(&newpathDirent)) {
                return e; // Unlink error
            }
        }

        if (auto e = newpathParent->link(oldnode, &newpathDirent)) {
            return e; // Link error
        }

        if (auto e = oldpathParent->unlink(&oldpathDirent)) {
            return e; // Unlink error
        }
    } else if (!oldnode->is_symlink()) { // Aight we have to copy it
        FsNode* oldpathParent = fs::ResolveParent(oldpath, olddir);
        assert(oldpathParent); // If this is null something went horribly wrong

        if (auto e = newpathParent->create(&newpathDirent, 0)) {
            return e; // Create error
        }

        newnode = ResolvePath(newpath, newdir);
        if (!newnode) {
            Log::Warning(
                "Filesystem: Rename: newpath was created with no error returned however it was unable to be found.");
            return Error{ENOENT};
        }

        uint8_t* buffer = (uint8_t*)kmalloc(oldnode->size);
        UIOBuffer uio = UIOBuffer(buffer);

        auto rret = oldnode->read(0, oldnode->size, &uio);
        if (rret.HasError()) {
            kfree(buffer);

            Log::Warning("Filesystem: Rename: Error reading oldpath");
            return rret.err;
        }

        auto wret = oldnode->write(0, rret.Value(), &uio);
        if (wret.HasError()) {
            kfree(buffer);

            Log::Warning("Filesystem: Rename: Error reading oldpath");
            return wret.err;
        }

        kfree(buffer);

        if (auto e = oldpathParent->unlink(&oldpathDirent)) {
            return e; // Unlink error
        }
    } else {
        Log::Warning("Filesystem: Rename: We do not support using rename on symlinks yet!"); // TODO: Rename the symlink
        return Error{ENOSYS};
    }

    return ERROR_NONE;
}
} // namespace fs
