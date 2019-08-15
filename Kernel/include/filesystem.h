#pragma once

#include <stdint.h>

#define FS_NODE_FILE 0x1
#define FS_NODE_DIRECTORY 0x2
#define FS_NODE_BLKDEVICE 0x3
#define FS_NODE_SYMLINK 0x4
#define FS_NODE_CHARDEVICE 0x5
#define FS_NODE_MOUNTPOINT 0x8

struct fs_node;

typedef uint32_t (*read_type_t) (struct fs_node *, uint32_t, uint32_t, uint8_t *);
typedef uint32_t (*write_type_t) (struct fs_node *, uint32_t, uint32_t, uint8_t *);
typedef void (*open_type_t) (struct fs_node*, uint32_t flags);
typedef void (*close_type_t) (struct fs_node*);
typedef struct fs_dirent *(*readdir_type_t) (struct fs_node*, uint32_t);
typedef struct fs_node*(*finddir_type_t) (struct fs_node*, char *name);

typedef struct fs_node{
    char name[128]; // Filename
    uint32_t flags; // Flags
    uint32_t pmask; // Permission mask
    uint32_t uid; // User id
    uint32_t inode; // Inode number
    uint32_t size; // Node size

    read_type_t read; // Read callback
    write_type_t write; // Write callback
    open_type_t open; // Open callback
    close_type_t close; // Close callback
    readdir_type_t readDir; // Read callback
    finddir_type_t findDir; // Find callback

    fs_node* ptr;
} fs_node_t;

typedef struct fs_dirent {
	uint32_t inode; // Inode number
	char name[128]; // Filename
    uint32_t type;
} fs_dirent_t;

namespace fs{
    uint32_t Read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer);
    uint32_t Write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer);
    void Open(fs_node_t* node, uint32_t flags);
    void Close(fs_node_t* node);
    fs_dirent_t* ReadDir(fs_node_t* node, uint32_t index);
    fs_node_t* FindDir(fs_node_t* node, char* name);
}