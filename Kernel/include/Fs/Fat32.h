#pragma once

#include <stdint.h>
#include <stddef.h>
#include <Device.h>
#include <Fs/Filesystem.h>
#include <Fs/FsVolume.h>

#define FAT_ATTR_READ_ONLY 0x1
#define FAT_ATTR_HIDDEN 0x2
#define FAT_ATTR_SYSTEM 0x4
#define FAT_ATTR_VOLUME_ID 0x8
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20

typedef struct {
    uint8_t jmp[3]; // Can be ignored
    int8_t oem[8]; // OEM identifier
    uint16_t bytesPerSector; // Bytes per sector
    uint8_t sectorsPerCluster; // Sectors per cluster
    uint16_t reservedSectors; // Reserved sectors count
    uint8_t fatCount; // Number of File Allocation Tables
    uint16_t directoryEntries; // Number of directory entries
    uint16_t volSectorCount; // FAT12/16 only
    uint8_t mediaDescriptorType; // Media Descriptor Type
    uint16_t sectorsPerFAT; // FAT12/16 only
    uint16_t sectorsPerTrack; // Sectors per track
    uint16_t headCount; // Number of heads on the storage device
    uint32_t hiddenSectorCount; // Number of hidden sectors
    uint32_t largeSectorCount; // Large sector count
} __attribute__((packed)) fat_bpb_t; // BIOS Parameter Block

typedef struct {
    uint32_t sectorsPerFAT; // Sectors per FAT
    uint16_t flags; // Flags
    uint16_t fatVersion; // FAT version number
    uint32_t rootClusterNum; // Cluster number of the root directory
    uint16_t fsInfoSector; // Sector number of the FSInfo structure
    uint16_t bkupBootSector; // Number of the backup boot sector
    uint8_t reserved[12]; // Reserved
    uint8_t driveNum; // 0x0 for floppy, 0x80 for hard disk
    uint8_t reserved1; // Reserved
    uint8_t signature; // Should be 0x28 or 0x29
    uint32_t volumeID; // Volume ID number
    int8_t volumeLabelString[11]; // Volume label string
    int8_t sysIdString[8]; // System identifier string ("FAT32   ")
} __attribute__((packed)) fat32_ebr_t; // FAT32 Extended Boot Record

typedef struct {
    fat_bpb_t bpb;
    fat32_ebr_t ebr;
} __attribute__((packed)) fat32_boot_record_t;

typedef struct {
    uint32_t leadSignature; // Should be 0x41615252
    uint8_t reserved[480];
    uint64_t signature; // Should be 0x61417272
    uint32_t freeClusterCount; // Last known free cluster count, if 0xFFFFFFFF then free clusters need to be recalculated, check if fits in cluster count
    uint32_t firstSearchCluster; // Search for free clusters from here, if 0xFFFFFFFF then from cluster 2, check if fits in cluster count
    uint8_t reserved2[12];
    uint16_t trailSignature; // Should be 0xAA55
} __attribute__((packed)) fat32_fsinfo_t; // FAT32 FSInfo Structure

typedef struct
{
    uint8_t filename[8];
    uint8_t ext[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t createTimeTenthsOfSecond;
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t accessedDate;
    uint16_t highClusterNum;
    uint16_t modificationTime;
    uint16_t modificationDate;
    uint16_t lowClusterNum;
    uint32_t fileSize;
} __attribute__((packed)) fat_entry_t;

typedef struct
{
    uint8_t order;
    uint16_t characters0[5];
    uint8_t attributes;
    uint8_t type;
    uint8_t checksum;
    uint16_t characters1[6];
    uint16_t zero;
    uint16_t characters2[2];
} __attribute__((packed)) fat_lfn_entry_t; // Long File Name

namespace fs::FAT32{
    class Fat32Volume;

    class Fat32Node : public FsNode {
    public:
        ssize_t Read(size_t, size_t, uint8_t *);
        ssize_t Write(size_t, size_t, uint8_t *);
        //fs_fd_t* Open(size_t flags);
        //void Close();
        int ReadDir(DirectoryEntry*, uint32_t);
        FsNode* FindDir(char* name);

        Fat32Volume* vol;
    };

    class Fat32Volume : public FsVolume {
    public:
        Fat32Volume(PartitionDevice* part, char* name);

        ssize_t Read(Fat32Node* node, size_t offset, size_t size, uint8_t *buffer);
        ssize_t Write(Fat32Node* node, size_t offset, size_t size, uint8_t *buffer);
        void Open(Fat32Node* node, uint32_t flags);
        void Close(Fat32Node* node);
        int ReadDir(Fat32Node* node, DirectoryEntry* dirent, uint32_t index);
        FsNode* FindDir(Fat32Node* node, char* name);

    private:
        uint64_t ClusterToLBA(uint32_t cluster);
        List<uint32_t>* GetClusterChain(uint32_t cluster);
        void* ReadClusterChain(uint32_t cluster, int* count);
        void* ReadClusterChain(uint32_t cluster, int* count, size_t max);

        PartitionDevice* part;
        fat32_boot_record_t* bootRecord;

        int clusterSizeBytes;
        Fat32Node fat32MountPoint;
    };

    int Identify(PartitionDevice* part);

    inline void GetLongFilename(char* name, fat_lfn_entry_t** lfnEntries, int lfnCount){
        int nameIndex = 0;
        for(int i = 0; i < lfnCount; i++){
            fat_lfn_entry_t* ent = lfnEntries[i];

            for(int j = 0; j < 5; j++){
                name[nameIndex++] = ent->characters0[j];
            }
            
            for(int j = 0; j < 6; j++){
               name[nameIndex++] = ent->characters1[j];
            }
            
            name[nameIndex++] = ent->characters2[0];
            name[nameIndex++] = ent->characters2[1];
            name[nameIndex++] = 0;
            name[nameIndex++] = 0;
        }
    }
}