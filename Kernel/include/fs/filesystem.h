#pragma once

#include <stdint.h>
#include <stddef.h>

#include <list.h>

#define PATH_MAX 4096

#define FS_NODE_FILE 0x1
#define FS_NODE_DIRECTORY 0x2
#define FS_NODE_MOUNTPOINT 0x8
#define FS_NODE_BLKDEVICE 0x10
#define FS_NODE_SYMLINK 0x20
#define FS_NODE_CHARDEVICE 0x40
#define FS_NODE_SOCKET 0x80

#define S_IFMT 0x0F000
#define S_IFBLK 0x06000
#define S_IFCHR 0x02000
#define S_IFIFO 0x01000
#define S_IFREG 0x08000
#define S_IFDIR 0x04000
#define S_IFLNK 0x0A000
#define S_IFSOCK 0x0C000

typedef int64_t ino_t;
typedef uint64_t dev_t;
typedef int32_t uid_t;
typedef int64_t off_t;
typedef int32_t mode_t;
typedef int32_t nlink_t;

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

class FsNode{
public:
    char name[128]; // Filename
    uint32_t flags = 0; // Flags
    uint32_t pmask = 0; // Permission mask
    uid_t uid = 0; // User id
    ino_t inode = 0; // Inode number
    size_t size = 0; // Node size

    virtual size_t Read(size_t, size_t, uint8_t *);
    virtual size_t Write(size_t, size_t, uint8_t *);
    virtual fs_fd_t* Open(size_t flags);
    virtual void Close();
    virtual int ReadDir(struct fs_dirent*, uint32_t);
    virtual FsNode* FindDir(char* name);
    virtual int Ioctl(uint64_t cmd, uint64_t arg);

    FsNode* link;
    FsNode* parent;
};

typedef struct fs_dirent {
	uint32_t inode; // Inode number
	char name[128]; // Filename
    uint32_t type;
} fs_dirent_t;

namespace fs{
    class FsVolume;

	extern List<FsVolume*>* volumes;

    void Initialize();
    FsNode* GetRoot();
    void RegisterDevice(FsNode* device);

    FsNode* ResolvePath(char* path, char* workingDir = nullptr);
    char* CanonicalizePath(char* path, char* workingDir);

    size_t Read(FsNode* node, size_t offset, size_t size, uint8_t *buffer);
    size_t Write(FsNode* node, size_t offset, size_t size, uint8_t *buffer);
    fs_fd_t* Open(FsNode* node, uint32_t flags = 0);
    void Close(FsNode* node);
    void Close(fs_fd_t* handle);
    int ReadDir(FsNode* node, fs_dirent_t* dirent, uint32_t index);
    FsNode* FindDir(FsNode* node, char* name);
    
    size_t Read(fs_fd_t* handle, size_t size, uint8_t *buffer);
    size_t Write(fs_fd_t* handle, size_t size, uint8_t *buffer);
    int ReadDir(fs_fd_t* handle, fs_dirent_t* dirent, uint32_t index);
    FsNode* FindDir(fs_fd_t* handle, char* name);

    int Ioctl(fs_fd_t* handle, uint64_t cmd, uint64_t arg);
}