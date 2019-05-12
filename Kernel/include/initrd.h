#pragma once

#include <stdint.h>
#include <filesystem.h>

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
    void Initialize(uint32_t address, uint32_t size);

    lemoninitfs_node_t* List();
    lemoninitfs_header_t* GetHeader();
    uint32_t Read(int node);
    uint32_t Read(char* filename);

    lemoninitfs_node_t GetNode(char* filename);
    uint8_t* Read(lemoninitfs_node_t node, uint8_t* buffer);
    fs_node_t* GetRoot();
    
	void RegisterDevice(fs_node_t* device);
}