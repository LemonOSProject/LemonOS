#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Error.h>
#include <List.h>
#include <Lock.h>
#include <MM/VMObject.h>
#include <Objects/File.h>
#include <Objects/KObject.h>
#include <RefPtr.h>
#include <String.h>
#include <Types.h>

#include <UserIO.h>

#include <abi/fcntl.h>
#include <abi/stat.h>

#include <Fs/DirectoryEntry.h>

#define FD_SETSIZE 1024

#define FS_NODE_TYPE 0xF000
#define FS_NODE_FILE S_IFREG
#define FS_NODE_DIRECTORY S_IFDIR
#define FS_NODE_MOUNTPOINT S_IFDIR
#define FS_NODE_BLKDEVICE S_IFBLK
#define FS_NODE_SYMLINK S_IFLNK
#define FS_NODE_CHARDEVICE S_IFCHR
#define FS_NODE_SOCKET S_IFSOCK

#define MAXIMUM_SYMLINK_AMOUNT 10

class FsNode;

typedef struct {
    char fds_bits[128];
} fd_set_t;

static inline void FD_CLR(int fd, fd_set_t* fds) {
    assert(fd < FD_SETSIZE);
    fds->fds_bits[fd / 8] &= ~(1 << (fd % 8));
}

static inline int FD_ISSET(int fd, fd_set_t* fds) {
    assert(fd < FD_SETSIZE);
    return fds->fds_bits[fd / 8] & (1 << (fd % 8));
}

static inline void FD_SET(int fd, fd_set_t* fds) {
    assert(fd < FD_SETSIZE);
    fds->fds_bits[fd / 8] |= 1 << (fd % 8);
}

class FilesystemWatcher;
struct DirectoryEntry;

class FsNode {
public:
    FileType type;

    uint32_t pmask = 0; // Permission mask
    uid_t uid = 0;      // User id
    gid_t gid = 0;
    ino_t inode = 0;          // Inode number
    size_t size = 0;          // Node size
    volume_id_t volumeID;

    int nlink = 0;            // Amount of references/hard links
    unsigned handleCount = 0; // Amount of files that point to this node

    virtual ~FsNode();

    /////////////////////////////
    /// \brief Read data from filesystem node
    ///
    /// Read data from filesystem node
    ///
    /// \param off Offset of data to read
    /// \param size Amount of data (in bytes) to read
    ///
    /// \return Bytes read or if negative an error code
    /////////////////////////////
    virtual ErrorOr<ssize_t> read(size_t off, size_t size, UIOBuffer* buffer); // Read Data

    /////////////////////////////
    /// \brief Write data to filesystem node
    ///
    /// Write data to filesystem node
    ///
    /// \param off Offset where data should be written
    /// \param size Amount of data (in bytes) to write
    ///
    /// \return Bytes written or if negative an error code
    /////////////////////////////
    virtual ErrorOr<ssize_t> write(size_t off, size_t size, UIOBuffer* buffer); // Write Data

    virtual ErrorOr<class File*> Open(size_t flags); // Open
    virtual void Close();                            // Close

    /////////////////////////////
    /// \brief Acquire directory entry at index
    ///
    /// \return 1 if an entry at index was found
    /// 0 if an entry at index doesn't exit
    /// Error on error
    /////////////////////////////
    virtual ErrorOr<int> read_dir(struct DirectoryEntry* ent, uint32_t index);
    virtual ErrorOr<FsNode*> find_dir(const char* name); // Find in directory

    virtual Error create(DirectoryEntry* ent, uint32_t mode);
    virtual Error create_directory(DirectoryEntry* ent, uint32_t mode);

    virtual ErrorOr<ssize_t> read_link(char* pathBuffer, size_t bufSize);
    virtual Error link(FsNode*, DirectoryEntry*);
    virtual Error unlink(DirectoryEntry*, bool unlinkDirectories = false);

    virtual Error truncate(off_t length);

    virtual ErrorOr<int> ioctl(uint64_t cmd, uint64_t arg); // I/O Control
    virtual void sync();                                    // Sync node to device

    virtual ErrorOr<MappedRegion*> mmap(uintptr_t base, size_t size, off_t off, int prot, bool shared, bool fixed);

    virtual bool can_read() { return true; }
    virtual bool can_write() { return true; }

    virtual inline bool is_file() { return type == FileType::Regular; }
    virtual inline bool is_directory() { return  type == FileType::Directory; }
    virtual inline bool is_block_dev() { return  type == FileType::BlockDevice; }
    virtual inline bool is_symlink() { return  type == FileType::SymbolicLink; }
    virtual inline bool is_char_dev() { return  type == FileType::CharDevice; }
    virtual inline bool is_socket() { return  type == FileType::Socket; }
    virtual inline bool is_epoll() const { return false; }
    virtual inline bool is_pipe() const { return false; }

    void unblock_all();

    lock_t nodeLock = 0;
};

/*class FilesystemBlocker : public ThreadBlocker {
    friend FsNode;
    friend FastList<FilesystemBlocker*>;

public:
    enum BlockType {
        BlockRead,
        BlockWrite,
    };

protected:
    FsNode* node = nullptr;

    FilesystemBlocker* next;
    FilesystemBlocker* prev;

    int blockType = BlockType::BlockRead;

    size_t requestedLength = 1; // How much data was requested?
public:
    FilesystemBlocker(FsNode* _node) : node(_node) {
        acquireLock(&lock);

        acquireLock(&node->blockedLock);
        node->blocked.add_back(this);
        releaseLock(&node->blockedLock);

        releaseLock(&lock);
    }

    FilesystemBlocker(FsNode* _node, size_t len) : node(_node), requestedLength(len) {
        acquireLock(&lock);

        acquireLock(&node->blockedLock);
        node->blocked.add_back(this);
        releaseLock(&node->blockedLock);

        releaseLock(&lock);
    }

    void Interrupt();

    inline void Unblock() {
        shouldBlock = false;

        acquireLock(&lock);
        if (node && !removed) { // It is assumed that the caller has acquired the node's blocked lock
            node->blocked.remove(this);

            removed = true;
        }
        node = nullptr;

        if (thread) {
            thread->Unblock();
        }
        releaseLock(&lock);
    }

    inline size_t RequestedLength() { return requestedLength; }

    ~FilesystemBlocker();
};*/

typedef struct fs_dirent {
    uint32_t inode; // Inode number
    uint32_t type;
    char name[NAME_MAX]; // Filename
} fs_dirent_t;
namespace fs {
class FsVolume;

class FsDriver {
public:
    virtual ~FsDriver() = default;

    /////////////////////////////
    /// \brief Mount
    /////////////////////////////
    virtual FsVolume* Mount(FsNode* device, const char* name) = 0;
    virtual FsVolume* Unmount(FsVolume* volume) = 0;

    virtual int Identify(FsNode* device) = 0;
    virtual const char* ID() const = 0;
};

extern List<FsDriver*> drivers;

void create_root_fs();

FsNode* root();

/////////////////////////////
/// \brief Register Kernel-level Filesystem Driver
///
/// This will allow partitions to be mounted, etc. using this driver
///
/// \param link Pointer to FsDriver object
/////////////////////////////
void RegisterDriver(FsDriver* driver);
void UnregisterDriver(FsDriver* driver);

FsDriver* IdentifyFilesystem(FsNode* node);

/////////////////////////////
/// \brief Follow symbolic link
///
/// \param link FsNode pointing to the link to be followed
/// \param workingDir FsNode pointing to working directory for path resolution
///
/// \return FsNode of node in which the link points to on success, nullptr on failure
/////////////////////////////
FsNode* FollowLink(FsNode* link, FsNode* workingDir);

/////////////////////////////
/// \brief Resolve a path.
///
/// \param path Path to resolve
/// \param workingDir Node of working directory
///
/// \return FsNode which path points to, nullptr on failure
/////////////////////////////
FsNode* ResolvePath(const String& path, FsNode* workingDir = nullptr, bool followSymlinks = true);

/////////////////////////////
/// \brief Resolve a path.
///
/// \param path Path to resolve
/// \param workingDir Path of working directory
///
/// \return FsNode which path points to, nullptr on failure
/////////////////////////////
FsNode* ResolvePath(const String& path, const char* workingDir, bool followSymlinks = true);

/////////////////////////////
/// \brief Resolve parent directory of path.
///
/// \param path Path of child for parent to resolve
/// \param workingDir Node of working directory
///
/// \return FsNode of parent, nullptr on failure
/////////////////////////////
FsNode* ResolveParent(const String& path, FsNode* workingDir = nullptr);
String CanonicalizePath(const char* path, char* workingDir);
String BaseName(const String& path);

/////////////////////////////
/// \brief Read data from filesystem node
///
/// Read data from filesystem node
///
/// \param node Pointer to node to read from
/// \param off Offset of data to read
/// \param size Amount of data (in bytes) to read
/// \param buffer Buffer to write into
///
/// \return Bytes read or if negative an error code
/////////////////////////////
ErrorOr<ssize_t> read(FsNode* node, size_t offset, size_t size, void* buffer);

/////////////////////////////
/// \brief Write data to filesystem node
///
/// Write data to filesystem node
///
/// \param node Pointer to node to write to
/// \param off Offset of write
/// \param size Amount of data (in bytes) to write
/// \param buffer Buffer to read from
///
/// \return Bytes written or if negative an error code
/////////////////////////////
ErrorOr<ssize_t> write(FsNode* node, size_t offset, size_t size, void* buffer);

ErrorOr<File*> Open(FsNode* node, uint32_t flags = 0);
void Close(FsNode* node);
void Close(File* openFile);
ErrorOr<int> read_dir(FsNode* node, DirectoryEntry* dirent, uint32_t index);
ErrorOr<FsNode*> find_dir(FsNode* node, const char* name);

ErrorOr<int> read_dir(const FancyRefPtr<File>& handle, DirectoryEntry* dirent, uint32_t index);
ErrorOr<FsNode*> find_dir(const FancyRefPtr<File>& handle, const char* name);
ErrorOr<int> ioctl(const FancyRefPtr<File>& handle, uint64_t cmd, uint64_t arg);

Error link(FsNode*, FsNode*, DirectoryEntry*);
Error Unlink(FsNode*, DirectoryEntry*, bool unlinkDirectories = false);

Error Rename(FsNode* olddir, const char* oldpath, FsNode* newdir, const char* newpath);
} // namespace fs
