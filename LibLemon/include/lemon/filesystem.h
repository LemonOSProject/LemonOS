#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>

#define FS_NODE_FILE 0x1
#define FS_NODE_DIRECTORY 0x2
#define FS_NODE_MOUNTPOINT 0x8
#define FS_NODE_BLKDEVICE 0x10
#define FS_NODE_SYMLINK 0x20
#define FS_NODE_CHARDEVICE 0x40

typedef struct lemon_dirent {
	uint32_t inode; // Inode number
	char name[128]; // Filename
    uint32_t type;
} lemon_dirent_t;

int lemon_open(const char* filename, int flags);
void lemon_close(int fd);
int lemon_read(int fd, void* buffer, size_t count);
int lemon_write(int fd, const void* buffer, size_t count);
off_t lemon_seek(int fd, off_t offset, int whence);
int lemon_readdir(int fd, uint64_t count, lemon_dirent_t* dirent);

#endif
