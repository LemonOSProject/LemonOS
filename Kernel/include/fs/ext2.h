#include <fs/filesystem.h>
#include <fs/fsvolume.h>
#include <device.h>
#include <hash.h>

#include <stdint.h>

#define EXT2_SUPERBLOCK_LOCATION 1024

#define EXT2_SUPER_MAGIC 0xEF53

#define EXT2_VALID_FS 1
#define EXT2_ERROR_FS 2

#define EXT2_GOOD_OLD_REV 0
#define EXT2_DYNAMIC_REV 1

#define EXT2_GOOD_OLD_INODE_SIZE 128

#define EXT2_S_IFMT  0xF000
#define EXT2_S_IFBLK 0x6000
#define EXT2_S_IFCHR 0x2000
#define EXT2_S_IFIFO 0x1000
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFLNK 0xA000
#define EXT2_S_IFSOCK 0xC000

#define EXT2_FT_UNKNOWN 0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR 2
#define EXT2_FT_CHRDEV 3
#define EXT2_FT_BLKDEV 4
#define EXT2_FT_FIFO 5
#define EXT2_FT_SOCK 6
#define EXT2_FT_SYMLINK 7

#define EXT2_ROOT_INODE_INDEX 2

#define EXT2_DIRECT_BLOCK_COUNT 12
#define EXT2_SINGLY_INDIRECT_INDEX 12
#define EXT2_DOUBLY_INDIRECT_INDEX 13
#define EXT2_TRIPLY_INDIRECT_INDEX 14

namespace fs::Ext2{
    enum ErrorAction{
        Continue = 1,       // Continue
        ReadOnly = 2,       // Mount readonly
        KernelPanic = 3,    // Kernel Panic
    };

    enum Ext2InodeType {
        BadInode = 1,
        RootInode = 2,
        ACLIndexInode = 3,
        ACLDataInode = 4,
        BootloaderInode = 5,
        UndeleteDirInode = 6,
    };

    enum CompatibleFeatures{
        DirectoryPreallocation = 0x1,   // Preallocate blocks for new directories
        ImagicInodes = 0x2,             // 
        Journal = 0x4,                  // Contains and Ext3 Journal
        ExtendedAttributes = 0x8,       // Extended Inode Attributes
        ResizeInode = 0x10,             //
        DirectoryIndexing = 0x20,       // HTree Directory Indexing
    };

    enum IncompatibleFeatures{
        Compression = 0x1,    // Filesystem Compression
        Filetype = 0x2,       // Filetype used
        Recover = 0x4,        // Ext3
        JournalDevice = 0x8,        // Ext3
        MetaBg = 0x10,
    };

    enum ReadonlyFeatures{
        Sparse = 0x1,       // Sparse Superblock (not stored in all block groups)
        LargeFiles = 0x2,   // 64-bit file size support
        BinaryTree = 0x4,   // Binary tree directory structure    
    };

    enum CreatorOS{
        Linux, // Linux
        HURD,  // GNU HURD
        MASIX, // MASIX
        FreeBSD, // FreeBSD
        Lites, // Lites
    };

    enum ErrorCode{
        NoError,
        DiskReadError,
        DiskWriteError,
        InvalidInodeError,
        FilesystemAccessError,
        IncompatibleError,
        MiscError,
    };

    #define EXT2_READONLY_FEATURE_SUPPORT (ReadonlyFeatures::Sparse | ReadonlyFeatures::LargeFiles)
    #define EXT2_INCOMPAT_FEATURE_SUPPORT (IncompatibleFeatures::Filetype)

    typedef struct {
        uint32_t inodeCount;        // Number of inodes (used + free) in the file system
        uint32_t blockCount;        // Number of blocks (used + free + reserved) in the filesystem
        uint32_t resvBlockCount;    // Number of blocks reserved for the superuser
        uint32_t freeBlockCount;    // Total number of free blocks (including superuser reserved)
        uint32_t freeInodeCount;    // Number of free inodes
        uint32_t firstDataBlock;    // First datablock / Block containing this superblock (0 if block size>1KB as the superblock is ALWAYS at 1KB)
        uint32_t logBlockSize;      // Block size is equal to (1024 << logBlockSize)
        uint32_t logFragSize;       // Fragment size is equal to (1024 << logFragSize)
        uint32_t blocksPerGroup;    // Number of blocks per group
        uint32_t fragsPerGroup;     // Number of fragments per group also used to determine block bitmap size
        uint32_t inodesPerGroup;    // Number of inodes per group
        uint32_t mountTime;         // UNIX timestamp of the last time the filesystem was mounted
        uint32_t writeTime;         // UNIX timestamp of the last time the filesystem was written to
        uint16_t mntCount;          // How many times the filesystem has been mounted since it was last verified
        uint16_t maxMntCount;       // The amount of times the filesystem should be mounted before a full check
        uint16_t magic;             // Filesystem  Identifier (equal to EXT2_SUPER_MAGIC (0xEF53))
        uint16_t state;             // When mounted set to EXT2_ERROR_FS, on unmount EXT2_VALID_FS. On mount if equal to EXT2_ERROR_FS then the fs was not cleanly unmounted and may contain errors
        uint16_t errors;            // What should the driver do when an error is detected?
        uint16_t minorRevLevel;     // Minor revision level
        uint32_t lastCheck;         // UNIX timestamp of the last filesystem check
        uint32_t checkInterval;     // Maximum UNIX time interval allowed between filesystem checks
        uint32_t creatorOS;         // ID of the OS that created the fs
        uint32_t revLevel;          // Revision level
        uint16_t resUID;            // Default user ID for reserved blocks
        uint16_t resGID;            // Default group ID for reserved blocks
    } __attribute__((packed)) ext2_superblock_t; // Ext2 superblock data

    typedef struct {
        uint32_t firstInode;        // First usable inode
        uint16_t inodeSize;         // Size of the inode structure
        uint16_t blockGroupNum;     // Inode structure size (always 128 in rev 0)
        uint32_t featuresCompat;    // Compatible Features bitmask  
        uint32_t featuresIncompat;  // Incompatible features bitmask (Do not mount)
        uint32_t featuresRoCompat;   // Features incompatible when writing bitmask (Mount as readonly)
        uint8_t uuid[16];           // volume UUID
        char volumeName[16];        // Volume Name
        char lastMounted[64];       // Path where the filesystem was last mounted
        uint32_t algorithmBitmap;   // Indicates compression algorithm
        uint8_t preallocatedBlocks; // Blocks to preallocate when a file is created
        uint8_t preallocdDirBlocks; // Blocks to preallocate when a directory is created
        uint16_t align;
    } __attribute__((packed)) ext2_superblock_extended_t; // Ext2 extended superblock

    typedef struct {
        uint32_t blockBitmap;       // Block ID of the block bitmap
        uint32_t inodeBitmap;       // Block ID of the inode bitmap
        uint32_t inodeTable;        // Block ID of the inode table
        uint16_t freeBlockCount;    // Number of free blocks in group
        uint16_t freeInodeCount;    // Number of free inodes in group
        uint16_t usedDirsCount;     // Number of inodes allocted to directories in group
        uint16_t padding;           // Padding
        uint8_t reserved[12];
    } __attribute__((packed)) ext2_blockgrp_desc_t; // Block group descriptor

    typedef struct {
        uint16_t mode;              // Indicates the format of the file
        uint16_t uid;               // User ID
        uint32_t size;              // Lower 32 bits of the file size
        uint32_t accessTime;        // Last access in UNIX time
        uint32_t createTime;        // Creation time in UNIX time
        uint32_t modTime;           // Modification time in UNIX time
        uint32_t deleteTime;        // Deletion time in UNIX time
        uint16_t gid;               // Group ID
        uint16_t linkCount;         // Amount of hard links (most inodes will have a count of 1)
        uint32_t blockCount;        // Number of 512 byte blocks reserved to the data of this inode
        uint32_t flags;             // How to access data
        uint32_t osd1;              // Operating System dependant value
        uint32_t blocks[15];     // Amount of blocks used to hold data, the first 12 entries are direct blocks, the 13th first indirect block, the 14th is the first doubly-indirect block and the 15th triply indirect
        uint32_t generation;        // Indicates the file version
        uint32_t fileACL;           // Extended attributes (only applies to rev 1)
        union{
            uint32_t dirACL;            // For regular files (in rev 1) this is the upper 32 bits of the filesize
            uint32_t sizeHigh;
        };
        uint32_t fragAddr;          // Location of the file fragment (obsolete)
        uint8_t osd2[12];
    } __attribute__((packed)) ext2_inode_t;

    typedef struct {
        uint32_t inode;             // Inode number
        uint16_t recordLength;      // Displacement to next directory entry/record (must be 4 byte aligned)
        uint8_t nameLength;         // Length of the filename (must not be  larger than (recordLength - 8))
        uint8_t fileType;           // Revision 1 only, indicates file type
        char name[];
    } __attribute__((packed)) ext2_directory_entry_t;

    class Ext2Volume;

    class Ext2Node : public FsNode{
    public:
        Ext2Node(Ext2Volume* vol) { this->vol = vol; }
        Ext2Node(Ext2Volume* vol, ext2_inode_t& ino, ino_t inode);

        size_t Read(size_t, size_t, uint8_t *);
        size_t Write(size_t, size_t, uint8_t *);
        int ReadDir(DirectoryEntry*, uint32_t);
        FsNode* FindDir(char* name);
        int Create(DirectoryEntry*, uint32_t);
        int CreateDirectory(DirectoryEntry*, uint32_t);

        void Close();
        void Sync();

        Ext2Volume* vol;
        ext2_inode_t e2inode;
    };

    class Ext2Volume : public FsVolume {
    private:
        PartitionDevice* part;

        struct {
            ext2_superblock_t super;
            ext2_superblock_extended_t superext;
        } __attribute__((packed));

        uint32_t superBlockIndex; // Block ID of the superblock 
        uint32_t rootInode = EXT2_ROOT_INODE_INDEX; // Inode number of the root directory
        uint32_t firstBlockGroupIndex; // Block ID of the first block group descriptor
        uint32_t blocksize; // Volume block size

        ext2_blockgrp_desc_t* blockGroups;
        uint32_t blockGroupCount;

        int error = false;
        bool readOnly = false;
        
        bool sparse, largeFiles, filetype;
        uint32_t inodeSize = 128;

        HashMap<uint32_t, Ext2Node*> inodeCache;

        inline uint32_t LocationToBlock(uint64_t l){
            return (l >> super.logBlockSize) >> 10;
        }

        inline uint64_t BlockToLBA(uint64_t block){
            return block * (blocksize / part->parentDisk->blocksize);
        }

        inline uint32_t ResolveInodeBlockGroup(uint32_t inode){
            return (inode - 1) / super.inodesPerGroup;
        }

        inline uint32_t ResolveInodeBlockGroupIndex(uint32_t inode){
            return (inode - 1) % super.inodesPerGroup;
        }

        inline uint64_t InodeLBA(uint32_t inode){
            uint32_t block = blockGroups[ResolveInodeBlockGroup(inode)].inodeTable;
            uint64_t lba = (block * blocksize + ResolveInodeBlockGroupIndex(inode) * inodeSize) / part->parentDisk->blocksize;

            return lba;
        }
        
        uint32_t GetInodeBlock(uint32_t index, ext2_inode_t& inode);
        void SetInodeBlock(uint32_t index, ext2_inode_t& inode, uint32_t block);

        int ReadInode(uint32_t num, ext2_inode_t& inode);
        int WriteInode(uint32_t num, ext2_inode_t& inode);

        int ReadBlock(uint32_t block, void* buffer);
        int WriteBlock(uint32_t block, void* buffer);

        Ext2Node* CreateNode();
        void EraseInode(ext2_inode_t& inode);

        uint32_t AllocateBlock();

        int ListDir(Ext2Node* node, List<DirectoryEntry>& entries);
        int WriteDir(Ext2Node* node, List<DirectoryEntry>& entries);
        int InsertDir(Ext2Node* node, List<DirectoryEntry>& entries);
        int InsertDir(Ext2Node* node, DirectoryEntry& ent);
    public:
        Ext2Volume(PartitionDevice* part, const char* name);

        size_t Read(Ext2Node* node, size_t offset, size_t size, uint8_t *buffer);
        size_t Write(Ext2Node* node, size_t offset, size_t size, uint8_t *buffer);
        int ReadDir(Ext2Node* node, DirectoryEntry* dirent, uint32_t index);
        FsNode* FindDir(Ext2Node* node, char* name);
        int Create(Ext2Node* node, DirectoryEntry* ent, uint32_t mode);
        int CreateDirectory(Ext2Node* node, DirectoryEntry* ent, uint32_t mode);

        void SyncNode(Ext2Node* node);
        void CleanNode(Ext2Node* node);

        int Error() { return error; }
    };
    
    int Identify(PartitionDevice* part);
}