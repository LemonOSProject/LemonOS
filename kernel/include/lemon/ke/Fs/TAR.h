#pragma once

#include <Fs/Filesystem.h>
#include <Fs/FsVolume.h>
#include <stdint.h>

#define TAR_TYPE_FILE '0'
#define TAR_TYPE_LINK_HARD '1'
#define TAR_TYPE_LINK_SYMBOLIC '2'
#define TAR_TYPE_CHARACTER_SPECIAL '3'
#define TAR_TYPE_BLOCK_SPECIAL '4'
#define TAR_TYPE_DIRECTORY '5'
#define TAR_TYPE_FIFO '6'
#define TAR_TYPE_FILE_CONTIGUOUS '7'
#define TAR_TYPE_GLOBAL_EXTENDED_HEADER 'g'
#define TAR_TYPE_EXTENDED_HEADER 'g'

typedef struct {
    char name[100];     // Filename
    char mode[8];       // File mode
    char uid[8];        // Owner user ID
    char gid[8];        // Owner group ID
    char size[12];      // File size
    char modTime[12];   // Last modification time
    char checksum[8];   // Checksum
    char type;          // Type flag
    char linkName[100]; // Link name
} __attribute__((packed)) tar_v7_header_t;

typedef struct {
    char name[100];           // Filename
    char mode[8];             // File mode
    char uid[8];              // Owner user ID
    char gid[8];              // Owner group ID
    char size[12];            // File size
    char modTime[12];         // Last modification time
    char checksum[8];         // Checksum
    char type;                // Type flag
    char linkName[100];       // Link name
    char sig[6];              // "ustar" (null terminated)
    char version[2];          // UStar version
    char username[32];        // Owner username
    char groupname[32];       // Owner groupname
    char devMajor[8];         // Device major number
    char devMinor[8];         // Device minor number
    char filenamePrefix[155]; // Filename prefix
} __attribute__((packed)) ustar_header_t;

typedef struct {
    union {
        ustar_header_t ustar;
        uint8_t block[512];
    };
} __attribute__((packed)) tar_header_t;

namespace fs::tar {
class TarVolume;

class TarNode : public FsNode {
public:
    char name[256];
    tar_header_t* header;

    ino_t parentInode;
    int entryCount;  // For Directories - Amount of child nodes
    ino_t* children; // For Directories - Inodes of children

    ErrorOr<ssize_t> read(size_t, size_t, UIOBuffer*);
    ErrorOr<ssize_t> write(size_t, size_t, UIOBuffer*);
    void Close();
    ErrorOr<int> read_dir(DirectoryEntry*, uint32_t);
    ErrorOr<FsNode*> find_dir(const char* name);

    TarVolume* vol;
};

class TarVolume : public FsVolume {
    tar_header_t* blocks;
    uint64_t blockCount = 0;

    uint64_t nodeCount = 1; // 0 is volume

    ino_t nextNode = 1;

    int ReadDirectory(int index, ino_t parent);
    void MakeNode(tar_header_t* header, TarNode* n, ino_t inode, ino_t parent, tar_header_t* dirHeader = nullptr);

public:
    TarNode* nodes;

    TarVolume(uintptr_t base, size_t size, char* name);

    ErrorOr<ssize_t> read(TarNode* node, size_t offset, size_t size, UIOBuffer* buffer);
    ErrorOr<ssize_t> write(TarNode* node, size_t offset, size_t size, UIOBuffer* buffer);
    void Open(TarNode* node, uint32_t flags);
    void Close(TarNode* node);
    ErrorOr<int> read_dir(TarNode* node, DirectoryEntry* dirent, uint32_t index);
    ErrorOr<FsNode*> find_dir(TarNode* node, const char* name);
};
}; // namespace fs::tar