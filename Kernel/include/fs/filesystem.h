#pragma once

#include <stdint.h>
#include <stddef.h>

#include <list.h>
#include <lock.h>

#include <types.h>

#define PATH_MAX 4096
#define NAME_MAX 255

#define S_IFMT 0xF000
#define S_IFBLK 0x6000
#define S_IFCHR 0x2000
#define S_IFIFO 0x1000
#define S_IFREG 0x8000
#define S_IFDIR 0x4000
#define S_IFLNK 0xA000
#define S_IFSOCK 0xC000

#define FS_NODE_TYPE 0xF000
#define FS_NODE_FILE S_IFREG
#define FS_NODE_DIRECTORY S_IFDIR//0x2
#define FS_NODE_MOUNTPOINT S_IFDIR//0x8
#define FS_NODE_BLKDEVICE S_IFBLK//0x10
#define FS_NODE_SYMLINK S_IFLNK//0x20
#define FS_NODE_CHARDEVICE S_IFCHR//0x40
#define FS_NODE_SOCKET S_IFSOCK//0x80

#define POLLIN 0x01
#define POLLOUT 0x02
#define POLLPRI 0x04
#define POLLHUP 0x08
#define POLLERR 0x10
#define POLLRDHUP 0x20
#define POLLNVAL 0x40
#define POLLWRNORM 0x80

#define O_ACCESS 7
#define O_EXEC 1
#define O_RDONLY 2
#define O_RDWR 3
#define O_SEARCH 4
#define O_WRONLY 5

#define O_APPEND 0x0008
#define O_CREAT 0x0010
#define O_DIRECTORY 0x0020
#define O_EXCL 0x0040
#define O_NOCTTY 0x0080
#define O_NOFOLLOW 0x0100
#define O_TRUNC 0x0200
#define O_NONBLOCK 0x0400
#define O_DSYNC 0x0800
#define O_RSYNC 0x1000
#define O_SYNC 0x2000
#define O_CLOEXEC 0x4000

#define POLLIN 0x01
#define POLLOUT 0x02
#define POLLPRI 0x04
#define POLLHUP 0x08
#define POLLERR 0x10
#define POLLRDHUP 0x20
#define POLLNVAL 0x40
#define POLLWRNORM 0x80

typedef int64_t ino_t;
typedef uint64_t dev_t;
typedef int32_t uid_t;
typedef int64_t off_t;
typedef int32_t mode_t;
typedef int32_t nlink_t;
typedef int64_t volume_id_t;

typedef struct {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	uid_t st_gid;
	dev_t st_rdev;
	off_t st_size;
	int64_t st_blksize;
	int64_t st_blocks;
} stat_t;

class FsNode;

typedef struct fs_fd{
    FsNode* node;
    off_t pos;
    mode_t mode;
} fs_fd_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};

class DirectoryEntry{
public:
    char name[NAME_MAX];

    FsNode* node = nullptr;
    uint32_t inode = 0;

    DirectoryEntry* parent = nullptr;

    mode_t flags = 0;

    DirectoryEntry(FsNode* node, const char* name) { this->node = node; strcpy(this->name, name); }
    DirectoryEntry() {}
};

class FsNode{
public:
    uint32_t flags = 0; // Flags
    uint32_t pmask = 0; // Permission mask
    uid_t uid = 0; // User id
    ino_t inode = 0; // Inode number
    size_t size = 0; // Node size
    int nlink = 0; // Amount of references/hard links
    unsigned handleCount = 0; // Amount of file handles that point to this node
    volume_id_t volumeID;

    int error = 0;

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
    virtual ssize_t Read(size_t off, size_t size, uint8_t* buffer); // Read Data
    
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
    virtual ssize_t Write(size_t off, size_t size, uint8_t* buffer); // Write Data

    virtual fs_fd_t* Open(size_t flags); // Open
    virtual void Close(); // Close

    virtual int ReadDir(DirectoryEntry*, uint32_t); // Read Directory
    virtual FsNode* FindDir(char* name); // Find in directory

    virtual int Create(DirectoryEntry*, uint32_t);
    virtual int CreateDirectory(DirectoryEntry*, uint32_t);
    
    virtual int Link(FsNode*, DirectoryEntry*);
    virtual int Unlink(DirectoryEntry*);
    
    virtual int Truncate(off_t length);

    virtual int Ioctl(uint64_t cmd, uint64_t arg); // I/O Control
    virtual void Sync(); // Sync node to device

    virtual bool CanRead() { return true; }
    virtual bool CanWrite() { return false; }

    FsNode* link;
    FsNode* parent;

    FilesystemLock nodeLock; // Lock on FsNode info
};

typedef struct fs_dirent {
	uint32_t inode; // Inode number
    uint32_t type;
	char name[NAME_MAX]; // Filename
} fs_dirent_t;

namespace fs{
    class FsVolume;

	extern List<FsVolume*>* volumes;

    void Initialize();
    FsNode* GetRoot();
    void RegisterDevice(DirectoryEntry* device);
	void RegisterVolume(FsVolume* vol);

    FsNode* ResolvePath(const char* path, const char* workingDir = nullptr);
    FsNode* ResolveParent(const char* path, const char* workingDir = nullptr);
    char* CanonicalizePath(const char* path, char* workingDir);
    char* BaseName(const char* path);

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
    ssize_t Read(FsNode* node, size_t offset, size_t size, uint8_t *buffer);
    
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
    ssize_t Write(FsNode* node, size_t offset, size_t size, uint8_t *buffer);
    fs_fd_t* Open(FsNode* node, uint32_t flags = 0);
    void Close(FsNode* node);
    void Close(fs_fd_t* handle);
    int ReadDir(FsNode* node, DirectoryEntry* dirent, uint32_t index);
    FsNode* FindDir(FsNode* node, char* name);
    
    ssize_t Read(fs_fd_t* handle, size_t size, uint8_t *buffer);
    ssize_t Write(fs_fd_t* handle, size_t size, uint8_t *buffer);
    int ReadDir(fs_fd_t* handle, DirectoryEntry* dirent, uint32_t index);
    FsNode* FindDir(fs_fd_t* handle, char* name);
    
    int Link(FsNode*, FsNode*, DirectoryEntry*);
    int Unlink(FsNode*, DirectoryEntry*);

    int Ioctl(fs_fd_t* handle, uint64_t cmd, uint64_t arg);

    int Rename(FsNode* olddir, char* oldpath, FsNode* newdir, char* newpath);
}