#pragma once

#include <stdint.h>
#include <Fs/Filesystem.h>
#include <Fs/FsVolume.h>

#define LEMONINITFS_FILE        0x01
#define LEMONINITFS_DIRECTORY   0x02
#define LEMONINITFS_SYMLINK     0x03
#define LEMONINITFS_MOUNTPOINT  0x04

typedef struct {
	uint32_t fileCount; // Amount of files present
	char versionString[16]; // Version string
} __attribute__((packed)) lemoninitfs_header_t;

typedef struct {
	uint16_t magic; // Check for corruption (should be 0xBEEF)
	char filename[32]; // Filename
	uint32_t offset; // Offset in file
	uint32_t size; // File Size
	uint8_t type;
} __attribute__((packed)) lemoninitfs_node_t ;

namespace Initrd{
    class InitrdVolume : public fs::FsVolume {
    public:
        InitrdVolume();

        size_t Read(struct fs_node* node, size_t offset, size_t size, uint8_t *buffer);
        size_t Write(struct fs_node* node, size_t offset, size_t size, uint8_t *buffer);
        void Open(struct fs_node* node, uint32_t flags);
        void Close(struct fs_node* node);
        int ReadDir(struct fs_node* node, struct fs_dirent* dirent, uint32_t index);
        fs_node* FindDir(struct fs_node* node, char* name);
    };

    void Initialize(uintptr_t address, uint32_t size);

    lemoninitfs_node_t* List();
    lemoninitfs_header_t* GetHeader();
    void* Read(int node);
    void* Read(char* filename);

    lemoninitfs_node_t GetNode(char* filename);
    uint8_t* Read(lemoninitfs_node_t node, uint8_t* buffer);
}