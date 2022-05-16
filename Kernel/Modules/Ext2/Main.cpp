#include "Ext2.h"

#include <Assert.h>
#include <Errno.h>
#include <Logging.h>
#include <Math.h>
#include <Module.h>

#include <Debug.h>

#ifdef EXT2_ENABLE_TIMER
#include <Timer.h>
#endif

namespace fs {

static int ModuleInit() {
    Ext2::Instance();

    return 0;
}

static int ModuleExit() {
    Ext2::Instance().~Ext2();

    return 0;
}

DECLARE_MODULE("ext2fs", "Ext2 Filesystem Driver", ModuleInit, ModuleExit);

lock_t Ext2::m_instanceLock = 0;
Ext2* Ext2::m_instance = nullptr;

Ext2::Ext2() { fs::RegisterDriver(this); }
Ext2::~Ext2() { fs::UnregisterDriver(this); }

Ext2& Ext2::Instance() {
    if (m_instance) {
        return *m_instance;
    }

    ScopedSpinLock lock(m_instanceLock);
    if (m_instance) {
        return *m_instance;
    }

    m_instance = new Ext2();
    return *m_instance;
}

FsVolume* Ext2::Mount(FsNode* device, const char* name) {
    Ext2Volume* vol = new Ext2Volume(device, name);
    if (vol->Error()) {
        delete vol;
        return nullptr; // Error mounting volume
    }

    m_extVolumes.add_back(vol);
    return vol;
}

FsVolume* Ext2::Unmount(FsVolume* volume) { assert(!"Ext2::Unmount is a stub!"); }

int Ext2::Identify(FsNode* device) {
    struct {
        ext2_superblock_t super;
        ext2_superblock_extended_t ext;
    } __attribute__((packed)) super;

    if (fs::Read(device, EXT2_SUPERBLOCK_LOCATION, sizeof(super), &super) != sizeof(super)) {
        return -1; // Disk Error
    }

    return (super.super.magic == EXT2_SUPER_MAGIC) && !(super.ext.featuresIncompat & (~EXT2_INCOMPAT_FEATURE_SUPPORT));
}

const char* Ext2::ID() const { return "ext2"; }

Ext2::Ext2Volume::Ext2Volume(FsNode* device, const char* name) {
    m_device = device;
    assert(device->IsCharDevice() || device->IsBlockDevice());

    if (fs::Read(m_device, EXT2_SUPERBLOCK_LOCATION, sizeof(ext2_superblock_t), &super) != sizeof(ext2_superblock_t)) {
        Log::Error("[Ext2] Disk Error Initializing Volume");
        error = DiskReadError;
        return; // Disk Error
    }

    if (super.revLevel) { // If revision level >= 0 grab the extended superblock as well
        if (fs::Read(m_device, EXT2_SUPERBLOCK_LOCATION, sizeof(ext2_superblock_t) + sizeof(ext2_superblock_extended_t),
                     &super) != sizeof(ext2_superblock_t) + sizeof(ext2_superblock_extended_t)) {
            Log::Error("[Ext2] Disk Error Initializing Volume");
            error = DiskReadError;
            return; // Disk Error
        }

        if ((superext.featuresIncompat & (~EXT2_INCOMPAT_FEATURE_SUPPORT)) != 0 ||
            (superext.featuresRoCompat & (~EXT2_READONLY_FEATURE_SUPPORT)) !=
                0) { // Check support for incompatible/read-only compatible features
            Log::Error(
                "[Ext2] Incompatible Ext2 features present (Incompt: %x, Read-only Compt: %x). Will not mount volume.",
                superext.featuresIncompat, superext.featuresRoCompat);
            error = IncompatibleError;
            return;
        }

        if (superext.featuresIncompat & IncompatibleFeatures::Filetype)
            filetype = true;
        else
            filetype = false;

        if (superext.featuresRoCompat & ReadonlyFeatures::LargeFiles)
            largeFiles = true;
        else
            largeFiles = false;

        if (superext.featuresRoCompat & ReadonlyFeatures::Sparse)
            sparse = true;
        else
            sparse = false;
    } else {
        memset(&superext, 0, sizeof(ext2_superblock_extended_t));
    }

    blockGroupCount = (super.blockCount % super.blocksPerGroup) ? (super.blockCount / super.blocksPerGroup + 1)
                                                                : (super.blockCount / super.blocksPerGroup); // Round up
    uint32_t secondBlockGroupCount = (super.inodeCount % super.inodesPerGroup)
                                         ? (super.inodeCount / super.inodesPerGroup + 1)
                                         : (super.inodeCount / super.inodesPerGroup); // Round up

    if (blockGroupCount != secondBlockGroupCount) {
        Log::Error("[Ext2] Block group count mismatch.");
        error = MiscError;
        return;
    }

    blocksize = 1024U << super.logBlockSize;
    superBlockIndex = LocationToBlock(EXT2_SUPERBLOCK_LOCATION);

    if (super.revLevel) {
        inodeSize = superext.inodeSize;
    } else {
        inodeSize = 128;
    }

    Log::Info("[Ext2] Initializing Volume\tRevision: %d, Block Size: %d, %d KB/%d KB used, Last mounted on: %s",
              super.revLevel, blocksize, super.freeBlockCount * blocksize / 1024, super.blockCount * blocksize / 1024,
              superext.lastMounted);

    if (debugLevelExt2 >= DebugLevelNormal) {
        Log::Info("[Ext2] Block Group Count: %d, Inodes Per Block Group: %d, Inode Size: %d", blockGroupCount,
                  super.inodesPerGroup, inodeSize);
        Log::Info("[Ext2] Sparse Superblock? %s Large Files? %s, Filetype Extension? %s", (sparse ? "Yes" : "No"),
                  (largeFiles ? "Yes" : "No"), (filetype ? "Yes" : "No"));
    }

    blockGroups = (ext2_blockgrp_desc_t*)kmalloc(blockGroupCount * sizeof(ext2_blockgrp_desc_t));

    uint64_t blockGroupOffset =
        BlockToLocation(LocationToBlock(EXT2_SUPERBLOCK_LOCATION) + 1); // One block from the superblock

    if (fs::Read(m_device, blockGroupOffset, blockGroupCount * sizeof(ext2_blockgrp_desc_t), blockGroups) !=
        blockGroupCount * sizeof(ext2_blockgrp_desc_t)) {
        Log::Error("[Ext2] Disk Error Initializing Volume");
        error = DiskReadError;
        return; // Disk Error
    }

    ext2_inode_t root;
    if (ReadInode(EXT2_ROOT_INODE_INDEX, root)) {
        Log::Error("[Ext2] Disk Error Initializing Volume");
        error = DiskReadError;
        return;
    }

    root.flags = EXT2_S_IFDIR;

    Ext2Node* e2mountPoint = new Ext2Node(this, root, EXT2_ROOT_INODE_INDEX);
    mountPoint = e2mountPoint;

    mountPointDirent.node = mountPoint;
    mountPointDirent.flags = DT_DIR;
    strcpy(mountPointDirent.name, name);
}

void Ext2::Ext2Volume::WriteSuperblock() {
    uint8_t buffer[blocksize];

    uint32_t superindex = LocationToBlock(EXT2_SUPERBLOCK_LOCATION);
    if (ReadBlockCached(superindex, buffer)) {
        Log::Info("[Ext2] WriteBlock: Error reading block %d", superindex);
        return;
    }

    memcpy(buffer + (EXT2_SUPERBLOCK_LOCATION % blocksize), &super,
           sizeof(ext2_superblock_t) + sizeof(ext2_superblock_extended_t));

    if (WriteBlockCached(superindex, buffer)) {
        Log::Info("[Ext2] WriteBlock: Error writing block %d", superindex);
        return;
    }
}

void Ext2::Ext2Volume::WriteBlockGroupDescriptor(uint32_t index) {
    uint32_t firstBlockGroup = LocationToBlock(EXT2_SUPERBLOCK_LOCATION) + 1;
    uint32_t block = firstBlockGroup + LocationToBlock(index * sizeof(ext2_blockgrp_desc_t));
    uint8_t buffer[blocksize];

    if (ReadBlockCached(block, buffer)) {
        Log::Info("[Ext2] WriteBlock: Error reading block %d", block);
        return;
    }

    memcpy(buffer + ((index * sizeof(ext2_blockgrp_desc_t)) % blocksize), &blockGroups[index],
           sizeof(ext2_blockgrp_desc_t));

    if (WriteBlockCached(block, buffer)) {
        Log::Info("[Ext2] WriteBlock: Error writing block %d", block);
        return;
    }
}

uint32_t Ext2::Ext2Volume::GetInodeBlock(uint32_t index, ext2_inode_t& ino) {
    uint32_t blocksPerPointer = blocksize / sizeof(uint32_t); // Amount of blocks in a indirect block table
    uint32_t singlyIndirectStart = EXT2_DIRECT_BLOCK_COUNT;
    uint32_t doublyIndirectStart = singlyIndirectStart + blocksPerPointer;
    uint32_t triplyIndirectStart = doublyIndirectStart + blocksPerPointer * blocksPerPointer;

    assert(index < triplyIndirectStart + blocksPerPointer * blocksPerPointer *
                                             blocksPerPointer); // Make sure that an invalid index was not passed

    if (index < EXT2_DIRECT_BLOCK_COUNT) {
        // Index lies within the direct blocklist
        return ino.blocks[index];
    } else if (index < doublyIndirectStart) {
        // Index lies within the singly indirect blocklist
        uint32_t buffer[blocksize / sizeof(uint32_t)];

        if (int e = ReadBlockCached(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)) {
            (void)e;
            error = DiskReadError;
            return 0;
        }

        return buffer[index - singlyIndirectStart];
    } else if (index < triplyIndirectStart) {
        // Index lies within the doubly indirect blocklist
        uint32_t blockPointers[blocksize / sizeof(uint32_t)];
        uint32_t buffer[blocksize / sizeof(uint32_t)];

        if (int e = ReadBlockCached(ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX], blockPointers)) {
            (void)e;
            error = DiskReadError;
            return 0;
        }

        uint32_t blockPointer = blockPointers[(index - doublyIndirectStart) / blocksPerPointer];

        if (int e = ReadBlockCached(blockPointer, buffer)) {
            (void)e;
            error = DiskReadError;
            return 0;
        }

        return buffer[(index - doublyIndirectStart) % blocksPerPointer];
    } else {
        assert(!"Yet to support triply indirect");
        return 0;
    }
}

Vector<uint32_t> Ext2::Ext2Volume::GetInodeBlocks(uint32_t index, uint32_t count, ext2_inode_t& ino) {
    uint32_t blocksPerPointer = blocksize / sizeof(uint32_t); // Amount of blocks in a indirect block table
    uint32_t singlyIndirectStart = EXT2_DIRECT_BLOCK_COUNT;
    uint32_t doublyIndirectStart = singlyIndirectStart + blocksPerPointer;
    uint32_t triplyIndirectStart = doublyIndirectStart + blocksPerPointer * blocksPerPointer;

    assert(index + count <
           triplyIndirectStart + blocksPerPointer * blocksPerPointer *
                                     blocksPerPointer); // Make sure that an invalid index was not passed

    unsigned i = index;
    Vector<uint32_t> blocks;
    blocks.reserve(count);

    while (i < EXT2_DIRECT_BLOCK_COUNT && i < index + count) {
        // Index lies within the direct blocklist
        blocks.add_back(ino.blocks[i++]);
    }

    if (i < doublyIndirectStart && i < index + count) {
        // Index lies within the singly indirect blocklist
        uint32_t buffer[blocksize / sizeof(uint32_t)];

        if (int e = ReadBlockCached(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)) {
            Log::Info("[Ext2] GetInodeBlocks: Error %i reading block %u (singly indirect block)", e,
                      ino.blocks[EXT2_SINGLY_INDIRECT_INDEX]);
            error = DiskReadError;

            blocks.clear();
            return blocks;
        }

        while (i < doublyIndirectStart && i < index + count) {
            uint32_t block = buffer[(i++) - singlyIndirectStart];
            if (block >= super.blockCount) {
                Log::Warning("[Ext2] GetInodeBlocks: Invalid singly indirect block %u, Singly indirect blocklist: %u",
                             block, ino.blocks[EXT2_SINGLY_INDIRECT_INDEX]);
            }
            blocks.add_back(block);
        }
    }

    if (i < triplyIndirectStart && i < index + count) {
        // Index lies within the doubly indirect blocklist
        uint32_t blockPointers[blocksize / sizeof(uint32_t)];
        uint32_t buffer[blocksize / sizeof(uint32_t)];

        if (int e = ReadBlockCached(ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX], blockPointers)) {
            Log::Info("[Ext2] GetInodeBlocks: Error %i reading block %u (doubly indirect block)", e,
                      ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX]);
            error = DiskReadError;

            blocks.clear();
            return blocks;
        }

        while (i < triplyIndirectStart && i < index + count) {
            uint32_t blockPointer = blockPointers[(i - doublyIndirectStart) / blocksPerPointer];

            if (int e = ReadBlockCached(blockPointer, buffer)) {
                Log::Info("[Ext2] GetInodeBlocks: Error %i reading block %u (doubly indirect block pointer)", e,
                          blockPointer);
                error = DiskReadError;

                blocks.clear();
                return blocks;
            }

            uint32_t blockPointerEnd = i + (blocksPerPointer - (i - doublyIndirectStart) % blocksPerPointer);
            while (i < blockPointerEnd && i < index + count) {
                uint32_t block = buffer[(i - doublyIndirectStart) % blocksPerPointer];
                if (block >= super.blockCount) {
                    Log::Warning("[Ext2] GetInodeBlocks: Invalid doubly indirect block %u", block);
                }

                blocks.add_back(block);
                i++;
            }
        }
    }

    if (i < index + count) {
        assert(!"[Ext2] Yet to support triply indirect blocklists");

        blocks.clear();
        return blocks;
    }

    return blocks;
}

void Ext2::Ext2Volume::SetInodeBlock(uint32_t index, ext2_inode_t& ino, uint32_t block) {
    uint32_t blocksPerPointer = blocksize / sizeof(uint32_t); // Amount of blocks in a indirect block table
    uint32_t singlyIndirectStart = EXT2_DIRECT_BLOCK_COUNT;
    uint32_t doublyIndirectStart = singlyIndirectStart + blocksPerPointer;
    uint32_t triplyIndirectStart = doublyIndirectStart + blocksPerPointer * blocksPerPointer;

    assert(index < triplyIndirectStart + blocksPerPointer * blocksPerPointer *
                                             blocksPerPointer); // Make sure that an invalid index was not passed

    if (index < EXT2_DIRECT_BLOCK_COUNT) {
        // Index lies within the direct blocklist
        ino.blocks[index] = block;
    } else if (index < doublyIndirectStart) {
        // Index lies within the singly indirect blocklist
        uint32_t buffer[blocksize / sizeof(uint32_t)];

        if (ino.blocks[EXT2_SINGLY_INDIRECT_INDEX] == 0) {
            ino.blocks[EXT2_SINGLY_INDIRECT_INDEX] = AllocateBlock();
        }

        if (int e = ReadBlockCached(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)) {
            (void)e;
            error = DiskReadError;
            return;
        }

        buffer[index - singlyIndirectStart] = block;

        if (int e = WriteBlockCached(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)) {
            (void)e;
            error = DiskWriteError;
            return;
        }
        return;
    } else if (index < triplyIndirectStart) {
        // Index lies within the doubly indirect blocklist
        uint32_t blockPointers[blocksize / sizeof(uint32_t)];
        uint32_t buffer[blocksize / sizeof(uint32_t)];

        if (int e = ReadBlockCached(ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX],
                                    blockPointers)) { // Read indirect block pointer list
            (void)e;
            error = DiskReadError;
            return;
        }

        uint32_t blockPointer = blockPointers[(index - doublyIndirectStart) / blocksPerPointer];

        if (int e = ReadBlockCached(blockPointer, buffer)) { // Read blocklist
            (void)e;
            error = DiskReadError;
            return;
        }

        buffer[(index - doublyIndirectStart) % blocksPerPointer] = block; // Update the index

        if (int e = WriteBlockCached(blockPointer, buffer)) { // Write our updated blocklist
            (void)e;
            error = DiskWriteError;
            return;
        }
        return;
    } else {
        assert(!"Yet to support triply indirect");
        return;
    }
}

int Ext2::Ext2Volume::ReadInode(uint32_t num, ext2_inode_t& inode) {
    uint8_t buf[512];

    if (int e = fs::Read(m_device, InodeOffset(num) & (~511U), 512, buf); e != 512) {
        Log::Error("[Ext2] Disk Error (%d) Reading Inode %d", e, num);
        error = DiskReadError;
        return e;
    }

    inode = *(ext2_inode_t*)(buf + ((ResolveInodeBlockGroupIndex(num) * inodeSize) & (512 - 1)));
    return 0;
}

int Ext2::Ext2Volume::ReadBlock(uint32_t block, void* buffer) {
    if (block > super.blockCount)
        return 1;

    if (int e = fs::Read(m_device, BlockToLocation(block), blocksize, buffer); e != blocksize) {
        Log::Error("[Ext2] Disk error (%d) reading block %d (blocksize: %d)", e, block, blocksize);
        return e;
    }

    return 0;
}

int Ext2::Ext2Volume::WriteBlock(uint32_t block, void* buffer) {
    if (block > super.blockCount)
        return 1;

    if (int e = fs::Write(m_device, BlockToLocation(block), blocksize, buffer); e != blocksize) {
        Log::Error("[Ext2] Disk error (%e) reading block %d (blocksize: %d)", e, block, blocksize);
        return e;
    }

    return 0;
}

int Ext2::Ext2Volume::ReadBlockCached(uint32_t block, void* buffer) {
    if (block > super.blockCount)
        return 1;

#ifndef EXT2_NO_CACHE
    ScopedSpinLock lockBlockCache(m_blocksLock);

    uint8_t* cachedBlock;
    if (blockCache.get(block, cachedBlock)) {
        memcpy(buffer, cachedBlock, blocksize);
    } else {
        cachedBlock = (uint8_t*)kmalloc(blocksize);
        if (int e = fs::Read(m_device, BlockToLocation(block), blocksize, cachedBlock); e != blocksize) {
            Log::Error("[Ext2] Disk error (%d) reading block %d (blocksize: %d)", e, block, blocksize);
            return e;
        }
        blockCache.insert(block, cachedBlock);
        blockCacheMemoryUsage += blocksize;
        Ext2::Instance().totalBlockCacheMemoryUsage += blocksize;

        memcpy(buffer, cachedBlock, blocksize);
    }
#else
    if (int e = fs::Read(m_device, BlockToLocation(block), blocksize, buffer); e != blocksize) {
        Log::Error("[Ext2] Disk error (%d) reading block %d (blocksize: %d)", e, block, blocksize);
        return e;
    }
#endif
    return 0;
}

int Ext2::Ext2Volume::WriteBlockCached(uint32_t block, void* buffer) {
    if (block > super.blockCount)
        return -1;

#ifndef EXT2_NO_CACHE
    ScopedSpinLock lockBlockCache(m_blocksLock);
    
    uint8_t* cachedBlock = nullptr;
    if ((blockCache.get(block, cachedBlock))) {
        memcpy(cachedBlock, buffer, blocksize);
    }
#endif

    if (int e = fs::Write(m_device, BlockToLocation(block), blocksize, buffer); e != blocksize) {
        Log::Error("[Ext2] Disk error (%d) reading block %d (blocksize: %d)", e, block, blocksize);
        return e;
    }

    return 0;
}

uint32_t Ext2::Ext2Volume::AllocateBlock() {
    for (unsigned i = 0; i < blockGroupCount; i++) {
        ext2_blockgrp_desc_t& group = blockGroups[i];

        if (group.freeBlockCount <= 0)
            continue; // No free blocks in this blockgroup

        uint8_t bitmap[blocksize / sizeof(uint8_t)];

        if (uint8_t * cachedBitmap; bitmapCache.get(group.blockBitmap, cachedBitmap)) {
            memcpy(bitmap, cachedBitmap, blocksize);
        } else {
            if (int e = ReadBlockCached(group.blockBitmap, bitmap)) {
                Log::Error("[Ext2] Disk error (%d) reading block bitmap (group %d)", e, i);
                error = DiskReadError;
                return 0;
            }
        }

        uint32_t block = 0;
        unsigned e = 0;
        for (; e < blocksize / sizeof(uint8_t); e++) {
            if (bitmap[e] == UINT8_MAX)
                continue; // Full

            for (uint8_t b = 0; b < sizeof(uint8_t) * 8; b++) { // Iterate through all bits in the entry
                if (((bitmap[e] >> b) & 0x1) == 0) {
                    bitmap[e] |= (1U << b);
                    block = (i * super.blocksPerGroup) + e * (sizeof(uint8_t) * 8) +
                            b; // Block Number = (Group number * blocks per group) + (bitmap entry * 8 (8 bits per
                               // byte)) + bit (block 0 is least significant bit, block 7 is most significant)
                    break;
                }
            }

            if (block)
                break;
        }

        if (!block) {
            continue;
        }

        if (uint8_t * cachedBitmap; bitmapCache.get(group.blockBitmap, cachedBitmap)) {
            memcpy(cachedBitmap, bitmap, blocksize);
        }

        if (int e = WriteBlockCached(group.blockBitmap, bitmap)) {
            Log::Error("[Ext2] Disk error (%d) write block bitmap (group %d)", e, i);
            error = DiskWriteError;
            return 0;
        }

        super.freeBlockCount--;
        blockGroups[i].freeBlockCount--;

        WriteBlockGroupDescriptor(i);
        WriteSuperblock();

        return block;
    }

    Log::Error("[Ext2] No space left on filesystem!");
    return 0;
}

int Ext2::Ext2Volume::FreeBlock(uint32_t block) {
    if (block > super.blockCount)
        return -1;

    ext2_blockgrp_desc_t& group = blockGroups[block / super.blocksPerGroup];

    uint8_t bitmap[blocksize / sizeof(uint8_t)];

    if (uint8_t * cachedBitmap; bitmapCache.get(group.blockBitmap, cachedBitmap)) {
        memcpy(bitmap, cachedBitmap, blocksize);
    } else {
        if (int e = ReadBlockCached(group.blockBitmap, bitmap)) {
            if (e == -EINTR) {
                return -EINTR;
            }

            Log::Error("[Ext2] Disk error (%d) reading block bitmap (group %d)", e, block / super.blocksPerGroup);
            error = DiskReadError;
            return -1;
        }
    }

    int bmapIndex = (block % super.blocksPerGroup) / (sizeof(uint8_t) * 8);
    int bitmask = ~(1 << (block % 8));
    bitmap[bmapIndex] &= bitmask;

    if (uint8_t * cachedBitmap; bitmapCache.get(group.blockBitmap, cachedBitmap)) {
        memcpy(cachedBitmap, bitmap, blocksize);
    }

    if (int e = WriteBlockCached(group.blockBitmap, bitmap)) {
        Log::Error("[Ext2] Disk error (%d) write block bitmap (group %d)", e, block / super.blocksPerGroup);
        error = DiskWriteError;
        return -1;
    }

    super.freeBlockCount++;
    blockGroups[block / super.blocksPerGroup].freeBlockCount++;

    WriteBlockGroupDescriptor(block / super.blocksPerGroup);
    WriteSuperblock();

    return 0;
}

Ext2::Ext2Node* Ext2::Ext2Volume::CreateNode() {
    ScopedSpinLock lockInodes(m_inodesLock);
    for (unsigned i = 0; i < blockGroupCount; i++) {
        ext2_blockgrp_desc_t& group = blockGroups[i];

        if (group.freeInodeCount <= 0)
            continue; // No free inodes in this blockgroup

        uint8_t bitmap[blocksize / sizeof(uint8_t)];

        if (int e = ReadBlockCached(group.inodeBitmap, bitmap)) {
            Log::Error("[Ext2] Disk error (%d) reading inode bitmap (group %d)", e, i);
            error = DiskReadError;
            return nullptr;
        }

        uint32_t inode = 0;

        for (unsigned e = 0; e < blocksize / sizeof(uint8_t); e++) {
            if (bitmap[e] == UINT8_MAX)
                continue; // Full

            for (uint8_t b = 0; b < sizeof(uint8_t) * 8; b++) { // Iterate through all bits in the entry
                if (((bitmap[e] >> b) & 0x1) == 0) {
                    inode = (i * super.inodesPerGroup) + e * (sizeof(uint8_t) * 8) + b +
                            1; // Inode Number = (Group number * inodes per group) + (bitmap entry * 8 (8 bits per
                               // byte)) + bit (inode 1 is least significant bit, block 8 is most significant) + 1
                               // (inodes start at 1)

                    if ((inode > superext.firstInode || super.revLevel == 0) && inode > 11) {
                        bitmap[e] |= (1U << b);
                        break;
                    }
                }
            }

            if (inode)
                break;
        }

        if (!inode)
            continue;

        if (int e = WriteBlockCached(group.inodeBitmap, bitmap)) {
            Log::Error("[Ext2] Disk error (%d) write inode bitmap (group %d)", e, i);
            error = DiskWriteError;
            return nullptr;
        }

        ext2_inode_t ino;

        memset(&ino, 0, sizeof(ext2_inode_t));

        ino.blocks[0] = AllocateBlock(); // Give it one block
        ino.uid = 0;
        ino.mode = 0644;
        ino.accessTime = ino.createTime = ino.deleteTime = ino.modTime = 0;
        ino.gid = 0;
        ino.blockCount = blocksize / 512;
        ino.linkCount = 0;
        ino.size = ino.sizeHigh = 0;
        ino.fragAddr = 0;
        ino.fileACL = 0;

        super.freeInodeCount--;
        blockGroups[i].freeInodeCount--;

        WriteSuperblock();
        WriteBlockGroupDescriptor(i);
        SyncInode(ino, inode);

        Ext2Node* node = new Ext2Node(this, ino, inode);

        if (debugLevelExt2 >= DebugLevelVerbose) {
            Log::Info("[Ext2] Created inode %d", node->inode);
        }

        return node;
    }

    Log::Error("[Ext2] No inodes left on the filesystem!");
    return nullptr;
}

int Ext2::Ext2Volume::EraseInode(ext2_inode_t& e2inode, uint32_t inode) {
    if (inode > super.inodeCount)
        return -1;

    if (e2inode.linkCount > 0) {
        Log::Warning("[Ext2] EraseInode: inode %d contains %d links! Will not erase.", inode, e2inode.linkCount);
        return -2;
    }

    for (unsigned i = 0; i < e2inode.blockCount * (blocksize / 512); i++) {
        uint32_t block = GetInodeBlock(i, e2inode);
        FreeBlock(block);
        if (blockCache.find(block)) {
            blockCache.remove(block);
        }
    }

    if (e2inode.blocks[EXT2_SINGLY_INDIRECT_INDEX]) {
        FreeBlock(e2inode.blocks[EXT2_SINGLY_INDIRECT_INDEX]);
        if (e2inode.blocks[EXT2_DOUBLY_INDIRECT_INDEX]) {
            uint32_t blockPointers[blocksize / sizeof(uint32_t)];

            if (int e = ReadBlockCached(e2inode.blocks[EXT2_DOUBLY_INDIRECT_INDEX], blockPointers)) {
                (void)e;
                error = DiskReadError;
                return 0;
            }

            for (unsigned i = 0; i < (blocksize / sizeof(uint32_t)) && blockPointers[i] != 0; i++) {
                FreeBlock(blockPointers[i]);
                if (blockCache.find(blockPointers[i])) {
                    blockCache.remove(blockPointers[i]);
                }
            }

            FreeBlock(e2inode.blocks[EXT2_DOUBLY_INDIRECT_INDEX]);

            if (e2inode.blocks[EXT2_TRIPLY_INDIRECT_INDEX]) {
                Log::Error("[Ext2] We do not support triply indirect blocks, will not free them.");
            }
        }
    }

    ext2_blockgrp_desc_t& group = blockGroups[inode / super.inodesPerGroup];

    uint8_t bitmap[blocksize / sizeof(uint8_t)];

    if (uint8_t * cachedBitmap; bitmapCache.get(group.inodeBitmap, cachedBitmap)) {
        memcpy(bitmap, cachedBitmap, blocksize);
    } else {
        if (int e = ReadBlockCached(group.inodeBitmap, bitmap)) {
            if (e == -EINTR) {
                return -EINTR;
            }

            Log::Error("[Ext2] Disk error (%d) reading inode bitmap (group %d)", e, inode / super.inodesPerGroup);
            error = DiskReadError;
            return -1;
        }
    }

    int bmapIndex = (inode % super.inodesPerGroup) / (sizeof(uint8_t) * 8);
    int bitmask = ~(1 << (inode % 8));
    bitmap[bmapIndex] &= bitmask;

    if (uint8_t * cachedBitmap; bitmapCache.get(group.inodeBitmap, cachedBitmap)) {
        memcpy(cachedBitmap, bitmap, blocksize);
    }

    if (int e = WriteBlockCached(group.blockBitmap, bitmap)) {
        Log::Error("[Ext2] Disk error (%d) write block bitmap (group %d)", e, inode / super.inodesPerGroup);
        error = DiskWriteError;
        return -1;
    }

    super.freeInodeCount++;
    blockGroups[inode / super.inodesPerGroup].freeInodeCount++;

    WriteBlockGroupDescriptor(inode / super.inodesPerGroup);
    WriteSuperblock();

    return 0;
}

int Ext2::Ext2Volume::ListDir(Ext2Node* node, List<DirectoryEntry>& entries) {
    if ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        Log::Warning("[Ext2] ListDir: Not a directory (inode %d)", node->inode);
        error = MiscError;
        return -ENOTDIR;
    }

    if (node->inode < 1) {
        Log::Warning("[Ext2] ListDir: Invalid inode: %d", node->inode);
        error = InvalidInodeError;
        return -1;
    }

    ext2_inode_t& ino = node->e2inode;

    uint8_t buffer[blocksize];
    uint32_t currentBlockIndex = 0;
    uint32_t blockOffset = 0;

    ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)buffer;

    if (int e = ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)) {
        if (e == -EINTR) {
            return -EINTR;
        }

        Log::Warning("[Ext2] ListDir: Error reading block %d", ino.blocks[currentBlockIndex]);
        error = DiskReadError;
        return -1;
    }

    for (;;) {
        if (e2dirent->recordLength == 0)
            break;

        if (e2dirent->inode > 0) { // 0 indicates unused
            DirectoryEntry dirent;
            dirent.flags = e2dirent->fileType;
            strncpy(dirent.name, e2dirent->name, e2dirent->nameLength);
            dirent.name[e2dirent->nameLength] = 0;
            dirent.inode = e2dirent->inode;

            entries.add_back(dirent);
        }

        blockOffset += e2dirent->recordLength;

        if (blockOffset >= blocksize) {
            currentBlockIndex++;

            if (currentBlockIndex > ino.blockCount / (blocksize / 512)) {
                // End of dir
                break;
            }

            blockOffset = 0;
            if (int e = ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)) {
                if (e == -EINTR) {
                    return -EINTR;
                }

                error = DiskReadError;
                return -1;
            }
        }

        e2dirent = (ext2_directory_entry_t*)(buffer + blockOffset);
    }

    return 0;
}

int Ext2::Ext2Volume::WriteDir(Ext2Node* node, List<DirectoryEntry>& entries) {
    if ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        return -ENOTDIR;
    }

    if (node->inode < 1) {
        Log::Warning("[Ext2] WriteDir: Invalid inode: %d", node->inode);
        return -1;
    }

    ext2_inode_t& ino = node->e2inode;

    uint8_t buffer[blocksize];
    uint32_t currentBlockIndex = 0;
    uint32_t blockOffset = 0;
    uint32_t totalOffset = 0;

    ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)kmalloc(sizeof(ext2_directory_entry_t) + NAME_MAX);

    unsigned lastIndex = entries.get_length() - 1;
    for (unsigned i = 0; i < entries.get_length(); i++) {
        DirectoryEntry ent = entries[i];

        if (debugLevelExt2 >= DebugLevelVerbose) {
            Log::Info("[Ext2] Writing entry: %s", ent.name);
        }

        e2dirent->fileType = ent.flags;
        e2dirent->inode = ent.inode;
        strncpy(e2dirent->name, ent.name, strlen(ent.name));
        e2dirent->nameLength = strlen(ent.name);

        if (i == lastIndex) {
            e2dirent->recordLength = blocksize - blockOffset;
        } else {
            if (strlen(entries[i + 1].name) + sizeof(ext2_directory_entry_t) + blockOffset > blocksize) {
                e2dirent->recordLength = blocksize - blockOffset; // Directory entries cannot span multiple blocks
            } else {
                e2dirent->recordLength = sizeof(ext2_directory_entry_t) + e2dirent->nameLength;
                if (e2dirent->recordLength && e2dirent->recordLength % 4) {
                    memset(((uint8_t*)e2dirent) + e2dirent->recordLength, 0,
                           4 - (e2dirent->recordLength % 4));                   // Pad with zeros
                    e2dirent->recordLength += 4 - (e2dirent->recordLength % 4); // Round up to nearest multiple of 4
                }
            }
        }

        memcpy((buffer + blockOffset), e2dirent, sizeof(ext2_directory_entry_t) + e2dirent->nameLength);
        blockOffset += e2dirent->recordLength;
        totalOffset += e2dirent->recordLength;

        if (blockOffset >= blocksize) {
            if (WriteBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)) {
                Log::Error("[Ext2] WriteDir: Failed to write directory block");
                error = DiskWriteError;
                return -1;
            }

            currentBlockIndex++;

            if (currentBlockIndex > ino.blockCount / (blocksize / 512)) {
                // Allocate a new block
                SetInodeBlock(currentBlockIndex, node->e2inode, AllocateBlock());
                node->e2inode.blockCount += blocksize / 512;
                WriteSuperblock();
            }

            blockOffset = 0;
        }

        node->e2inode.size = totalOffset;
        node->size = node->e2inode.size;
    }

    kfree(e2dirent);

    SyncNode(node);

    return 0;
}

int Ext2::Ext2Volume::InsertDir(Ext2Node* node, List<DirectoryEntry>& newEntries) {
    List<DirectoryEntry> entries;
    ListDir(node, entries);

    for (DirectoryEntry& newEnt : newEntries) {
        for (DirectoryEntry& ent : entries) {
            if (strcmp(ent.name, newEnt.name) == 0) {
                Log::Warning("[Ext2] InsertDir: Entry %s already exists!", newEnt.name);
                return -EEXIST;
            }
        }
        entries.add_back(newEnt);
    }

    return WriteDir(node, entries);
}

int Ext2::Ext2Volume::InsertDir(Ext2Node* node, DirectoryEntry& ent) {
    List<DirectoryEntry> ents;
    ents.add_back(ent);

    return InsertDir(node, ents);
}

int Ext2::Ext2Volume::ReadDir(Ext2Node* node, DirectoryEntry* dirent, uint32_t index) {
    if ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        return -ENOTDIR;
    }

    if (node->inode < 1) {
        Log::Warning("[Ext2] ReadDir: Invalid inode: %d", node->inode);
        return -EIO;
    }

    ext2_inode_t& ino = node->e2inode;

    uint8_t buffer[blocksize];
    uint32_t currentBlockIndex = 0;
    uint32_t blockOffset = 0;

    ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)buffer;

    if (ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)) {
        Log::Warning("[Ext2] Failed to read block %d", GetInodeBlock(currentBlockIndex, ino));
        error = DiskReadError;
        return -EIO;
    }

    for (unsigned i = 0; i < index; i++) {
        if (e2dirent->recordLength == 0)
            return 0;

        blockOffset += e2dirent->recordLength;

        if (e2dirent->recordLength < 8) {
            IF_DEBUG(debugLevelExt2 >= DebugLevelNormal, {
                Log::Warning("[Ext2] Error (inode: %d) record length of directory entry is invalid (value: %d)!",
                             node->inode, e2dirent->recordLength);
            });
            break;
        }

        if (e2dirent->inode == 0) {
            index--; // Ignore the entry
        }

        if (blockOffset >= blocksize) {
            currentBlockIndex++;

            if (currentBlockIndex >= ino.blockCount / (blocksize / 512)) {
                // End of dir
                return 0;
            }

            blockOffset = 0;
            if (ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)) {
                Log::Warning("[Ext2] Failed to read block");
                return -EIO;
            }
        }

        e2dirent = (ext2_directory_entry_t*)(buffer + blockOffset);
    }

    strncpy(dirent->name, e2dirent->name, e2dirent->nameLength);
    dirent->name[e2dirent->nameLength] = 0; // Null terminate
    dirent->flags = e2dirent->fileType;

    switch (e2dirent->fileType) {
    case EXT2_FT_REG_FILE:
        dirent->flags = DT_REG;
        break;
    case EXT2_FT_DIR:
        dirent->flags = DT_DIR;
        break;
    case EXT2_FT_CHRDEV:
        dirent->flags = DT_CHR;
        break;
    case EXT2_FT_BLKDEV:
        dirent->flags = DT_BLK;
        break;
    case EXT2_FT_FIFO:
        dirent->flags = DT_FIFO;
        break;
    case EXT2_FT_SOCK:
        dirent->flags = DT_SOCK;
        break;
    case EXT2_FT_SYMLINK:
        dirent->flags = DT_LNK;
        break;
    }

    return 1;
}

FsNode* Ext2::Ext2Volume::FindDir(Ext2Node* node, const char* name) {
    if ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        return nullptr;
    }

    if (node->inode < 1) {
        Log::Warning("[Ext2] ReadDir: Invalid inode: %d", node->inode);
        return nullptr;
    }

    ext2_inode_t& ino = node->e2inode;

    uint8_t buffer[blocksize];
    uint32_t currentBlockIndex = 0;
    uint32_t blockOffset = 0;
    uint32_t totalOffset = 0;

    ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)buffer;

    if (ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)) {
        Log::Info("[Ext2] Failed to read block %d", GetInodeBlock(currentBlockIndex, ino));
        return nullptr;
    }

    while (currentBlockIndex < ino.blockCount / (blocksize / 512)) {
        if (e2dirent->recordLength < 8) {
            IF_DEBUG(debugLevelExt2 >= DebugLevelNormal, {
                Log::Warning("[Ext2] Error (inode: %d) record length of directory entry is invalid (value: %d)!",
                             node->inode, e2dirent->recordLength);
            });
            break;
        }

        if (e2dirent->inode > 0) {
            IF_DEBUG(debugLevelExt2 >= DebugLevelVerbose, {
                char buf[e2dirent->nameLength + 1];
                strncpy(buf, e2dirent->name, e2dirent->nameLength);
                buf[e2dirent->nameLength] = 0;
                Log::Info("Checking name '%s' (name len %d), inode %d, len %d (parent inode: %d)", buf,
                          e2dirent->nameLength, e2dirent->inode, e2dirent->recordLength, node->inode);
            });

            if (strlen(name) == e2dirent->nameLength && strncmp(name, e2dirent->name, e2dirent->nameLength) == 0) {
                Log::Debug(debugLevelExt2, DebugLevelVerbose, "Found '%s'!", name);
                break;
            }
        }

        blockOffset += e2dirent->recordLength;
        totalOffset += e2dirent->recordLength;

        if (totalOffset > ino.size)
            return nullptr;

        if (blockOffset >= blocksize) {
            currentBlockIndex++;

            if (currentBlockIndex >= ino.blockCount / (blocksize / 512)) {
                // End of dir
                return nullptr;
            }

            blockOffset = 0;

            if (ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)) {
                Log::Error("[Ext2] Failed to read block");
                return nullptr;
            }
        }

        e2dirent = (ext2_directory_entry_t*)(buffer + blockOffset);
    }

    if (strlen(name) != e2dirent->nameLength || strncmp(e2dirent->name, name, e2dirent->nameLength) != 0) {
        // Not found
        return nullptr;
    }

    if (!e2dirent->inode || e2dirent->inode > super.inodeCount) {
        Log::Error("[Ext2] Directory Entry %s contains invalid inode %d", name, e2dirent->inode);
        return nullptr;
    }

    Ext2Node* returnNode = nullptr;

    ScopedSpinLock lockInodes(m_inodesLock);
    if (!inodeCache.get(e2dirent->inode, returnNode) || !returnNode) { // Could not locate inode in cache
        ext2_inode_t direntInode;
        if (ReadInode(e2dirent->inode, direntInode)) {
            Log::Error("[Ext2] Failed to read inode of directory (inode %d) entry %s", node->inode, e2dirent->name);
            return nullptr; // Could not read inode
        }

        IF_DEBUG(debugLevelExt2 >= DebugLevelNormal,
                 { Log::Info("[Ext2] FindDir: Opening inode %d, size: %d", e2dirent->inode, direntInode.size); });

        returnNode = new Ext2Node(this, direntInode, e2dirent->inode);

        inodeCache.insert(e2dirent->inode, returnNode);
    }

    assert(returnNode);
    return returnNode;
}

ssize_t Ext2::Ext2Volume::Read(Ext2Node* node, size_t offset, size_t size, uint8_t* buffer) {
    if (offset >= node->size)
        return 0;
    if (offset + size > node->size)
        size = node->size - offset;

    uint32_t blockIndex = LocationToBlock(offset);
    uint32_t blockLimit = LocationToBlock(offset + size);
    uint8_t blockBuffer[blocksize];

    /*if(debugLevelExt2 >= DebugLevelVerbose){
        Log::Info("[Ext2] Reading: Block index: %d, Block limit: %d, Offset: %d, Size: %d, Node size: %d", blockIndex,
    blockLimit, offset, size, node->size);
    }*/

#ifdef EXT2_ENABLE_TIMER
    timeval_t blktv1 = Timer::GetSystemUptimeStruct();
#endif

    ssize_t ret = size;
    Vector<uint32_t> blocks = GetInodeBlocks(blockIndex, blockLimit - blockIndex + 1, node->e2inode);
    assert(blocks.size() == (blockLimit - blockIndex + 1));

#ifdef EXT2_ENABLE_TIMER
    timeval_t blktv2 = Timer::GetSystemUptimeStruct();
    timeval_t readtv1 = Timer::GetSystemUptimeStruct();
#endif

    for (uint32_t block : blocks) {
        if (size <= 0)
            break;

        if (offset && offset % blocksize) {
            if (int e = ReadBlockCached(block, blockBuffer); e) {
                if (int e = ReadBlockCached(block, blockBuffer); e) { // Try again
                    Log::Info("[Ext2] Error %i reading block %u", e, block);
                    error = DiskReadError;
                    break;
                }
            }

            size_t readSize = blocksize - (offset % blocksize);
            if (readSize > size) {
                readSize = size;
            }

            size_t readOffset = (offset % blocksize);

            if (readSize > size)
                readSize = size;

            memcpy(buffer, blockBuffer + readOffset, readSize);

            size -= readSize;
            buffer += readSize;
            offset += readSize;
        } else if (size >= blocksize) {
            if (int e = ReadBlockCached(block, buffer); e) {
                if (int e = ReadBlockCached(block, buffer); e) { // Try again
                    Log::Info("[Ext2] Error %i reading block %u", e, block);
                    error = DiskReadError;
                    break;
                }
            }
            size -= blocksize;
            buffer += blocksize;
            offset += blocksize;
        } else {
            if (int e = ReadBlockCached(block, blockBuffer); e) {
                if (int e = ReadBlockCached(block, blockBuffer); e) { // Try again
                    Log::Info("[Ext2] Error %i reading block %u", e, block);
                    error = DiskReadError;
                    break;
                }
            }
            memcpy(buffer, blockBuffer, size);

            size = 0;
            break;
        }
    }

#ifdef EXT2_ENABLE_TIMER
    timeval_t readtv2 = Timer::GetSystemUptimeStruct();

    Log::Info("[Ext2] Retrieving inode blocks took %d ms, Reading inode blocks took %d ms",
              Timer::TimeDifference(blktv2, blktv1), Timer::TimeDifference(readtv2, readtv1));
#endif

    if (size) {
        if (debugLevelExt2 >= DebugLevelVerbose) {
            Log::Info("[Ext2] Requested %d bytes, read %d bytes (offset: %d)", ret, ret - size, offset);
        }
        return ret - size;
    }

    return ret;
}

ssize_t Ext2::Ext2Volume::Write(Ext2Node* node, size_t offset, size_t size, uint8_t* buffer) {
    if (readOnly) {
        error = FilesystemAccessError;
        return -EROFS;
    }

    if ((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY) {
        return -EISDIR;
    }

    uint32_t blockIndex = LocationToBlock(offset);                          // Index of first block to write
    uint32_t fileBlockCount = node->e2inode.blockCount / (blocksize / 512); // Size of file in blocks
    uint32_t blockLimit = LocationToBlock(offset + size);                   // Amount of blocks to write
    uint8_t blockBuffer[blocksize];                                         // block buffer
    bool sync = false;                                                      // Need to sync the inode?

    if (blockLimit >= fileBlockCount) {
        if (debugLevelExt2 >= DebugLevelVerbose) {
            Log::Info("[Ext2] Allocating blocks for inode %d", node->inode);
        }
        for (unsigned i = fileBlockCount; i <= blockLimit; i++) {
            uint32_t block = AllocateBlock();
            SetInodeBlock(i, node->e2inode, block);
        }
        node->e2inode.blockCount = (blockLimit + 1) * (blocksize / 512);

        WriteSuperblock();
        sync = true;
    }

    if (offset + size > node->size) {
        node->size = offset + size;
        node->e2inode.size = node->size;

        sync = true;
    }

    if (sync) {
        SyncNode(node);
    }

    if (debugLevelExt2 >= DebugLevelVerbose) {
        Log::Info("[Ext2] Writing: Block index: %d, Blockcount: %d, Offset: %d, Size: %d", blockIndex,
                  blockLimit - blockIndex + 1, offset, size);
    }

    ssize_t ret = size;
    Vector<uint32_t> blocks = GetInodeBlocks(blockIndex, blockLimit - blockIndex + 1, node->e2inode);

    for (uint32_t block : blocks) {
        if (size <= 0)
            break;

        if (offset % blocksize) {
            ReadBlockCached(block, blockBuffer);

            size_t writeSize = blocksize - (offset % blocksize);
            size_t writeOffset = (offset % blocksize);

            if (writeSize > size)
                writeSize = size;

            memcpy(blockBuffer + writeOffset, buffer, writeSize);
            if (int e = WriteBlockCached(block, blockBuffer); e) {
                if (int e = WriteBlockCached(block, blockBuffer); e) { // Try again
                    Log::Warning("[Ext2] Error %i writing block %u", e, block);
                    error = DiskReadError;
                    break;
                }
            }

            size -= writeSize;
            buffer += writeSize;
            offset += writeSize;
        } else if (size >= blocksize) {
            memcpy(blockBuffer, buffer, blocksize);
            if (int e = WriteBlockCached(block, blockBuffer); e) {
                if (int e = WriteBlockCached(block, blockBuffer); e) { // Try again
                    Log::Warning("[Ext2] Error %i writing block %u", e, block);
                    error = DiskReadError;
                    break;
                }
            }

            size -= blocksize;
            buffer += blocksize;
            offset += blocksize;
        } else {
            if (int e = ReadBlockCached(block, blockBuffer); e) {
                if (int e = ReadBlockCached(block, blockBuffer); e) { // Try again
                    Log::Info("[Ext2] Error %i reading block %u", e, block);
                    error = DiskReadError;
                    break;
                }
            }

            memcpy(blockBuffer, buffer, size);

            if (int e = WriteBlockCached(block, blockBuffer); e) {
                if (int e = WriteBlockCached(block, blockBuffer); e) { // Try again
                    Log::Warning("[Ext2] Error %i writing block %u", e, block);
                    error = DiskReadError;
                    break;
                }
            }

            buffer += size;
            offset += size;
            size = 0;
            break;
        }
    }

    if (size > 0) {
        if (debugLevelExt2 >= DebugLevelVerbose) {
            Log::Info("[Ext2] Tried to write %d bytes, wrote %d bytes (offset: %d)", ret, ret - size, offset);
        }
        ret -= size;
    }

    return ret;
}

void Ext2::Ext2Volume::SyncInode(ext2_inode_t& e2inode, uint32_t inode) {
    uint8_t buf[blocksize];
    uint64_t off = InodeOffset(inode);

    if (int e = fs::Read(m_device, off & (~511U), blocksize, buf); e != blocksize) {
        Log::Error("[Ext2] Sync: Disk Error (%d) Reading Inode %d", e, inode);
        error = DiskReadError;
        return;
    }

    *(ext2_inode_t*)(buf + (off & (511U))) = e2inode;

    if (int e = fs::Write(m_device, off & (~511U), blocksize, buf); e != blocksize) {
        Log::Error("[Ext2] Sync: Disk Error (%d) Writing Inode %d", e, inode);
        error = DiskWriteError;
        return;
    }
}

void Ext2::Ext2Volume::SyncNode(Ext2Node* node) {
    ScopedSpinLock lockInodes(m_inodesLock);
    SyncInode(node->e2inode, node->inode);
}

int Ext2::Ext2Volume::Create(Ext2Node* node, DirectoryEntry* ent, uint32_t mode) {
    if ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY)
        return -ENOTDIR; // Ensure the directory node is actually a directory

    // Make sure the filename does not already exist under the directory
    if (FindDir(node, ent->name)) {
        Log::Info("[Ext2] Create: Entry %s already exists!", ent->name);
        return -EEXIST;
    }

    // Create new inode in the VFS for the new file
    Ext2Node* file = CreateNode();

    if (!file) {
        return -EIO;
    }

    // Regular file
    file->e2inode.mode = EXT2_S_IFREG;
    file->flags = FS_NODE_FILE;
    // File is only references by one directory
    file->nlink = 1;
    file->e2inode.linkCount = 1;
    inodeCache.insert(file->inode, file);

    // Update the directory entry with the inode
    ent->node = file;
    ent->inode = file->inode;
    ent->flags = EXT2_FT_REG_FILE;

    // Sync inode to disk
    SyncNode(file);

    // Add the new entry in the given directory
    return InsertDir(node, *ent);
}

int Ext2::Ext2Volume::CreateDirectory(Ext2Node* node, DirectoryEntry* ent, uint32_t mode) {
    if (readOnly) {
        return -EROFS;
    }

    if ((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
        Log::Info("[Ext2] Not a directory!");
        return -ENOTDIR;
    }

    if (FindDir(node, ent->name)) {
        Log::Info("[Ext2] CreateDirectory: Entry %s already exists!", ent->name);
        return -EEXIST;
    }

    Ext2Node* dir = CreateNode();

    if (!dir) {
        Log::Error("[Ext2] Could not create inode!");
        return -EIO;
    }

    dir->e2inode.mode = EXT2_S_IFDIR;
    dir->flags = FS_NODE_DIRECTORY;
    dir->e2inode.linkCount = 1;

    inodeCache.insert(dir->inode, dir);
    ent->node = dir;
    ent->inode = dir->inode;
    ent->flags = EXT2_FT_DIR;

    DirectoryEntry currentEnt;
    strcpy(currentEnt.name, ".");
    currentEnt.node = dir;
    currentEnt.inode = dir->inode;
    currentEnt.flags = EXT2_FT_DIR;
    dir->e2inode.linkCount++;

    DirectoryEntry parentEnt;
    strcpy(parentEnt.name, "..");
    parentEnt.node = node;
    parentEnt.inode = node->inode;
    parentEnt.flags = EXT2_FT_DIR;
    node->e2inode.linkCount++;

    List<DirectoryEntry> entries;
    entries.add_back(currentEnt);
    entries.add_back(parentEnt);

    if (int e = InsertDir(dir, entries)) {
        return e;
    }

    if (int e = InsertDir(node, *ent)) {
        return e;
    }

    dir->nlink = dir->e2inode.linkCount;
    node->nlink = node->e2inode.linkCount;
    SyncNode(dir);
    SyncNode(node);

    return 0;
}

ssize_t Ext2::Ext2Volume::ReadLink(Ext2Node* node, char* pathBuffer, size_t bufSize) {
    if ((node->flags & S_IFMT) != S_IFLNK) {
        return -EINVAL; // Not a symbolic link
    }

    if (node->size <= 60) {
        size_t copySize = MIN(bufSize, node->size);
        strncpy(pathBuffer, (const char*)node->e2inode.blocks,
                copySize); // Symlinks up to 60 bytes are stored in the blocklist

        return copySize;
    } else {
        return this->Read(node, 0, bufSize, (uint8_t*)pathBuffer);
    }
}

int Ext2::Ext2Volume::Link(Ext2Node* node, Ext2Node* file, DirectoryEntry* ent) {
    ent->inode = file->inode;
    if (!ent->inode) {
        Log::Error("[Ext2] Link: Invalid inode %d", ent->inode);
        return -EINVAL;
    }

    if (file->volumeID != volumeID) {
        return -EXDEV; // Different filesystem
    }

    List<DirectoryEntry> entries;
    if (int e = ListDir(node, entries)) {
        Log::Error("[Ext2] Link: Error listing directory!", ent->inode);
        return e;
    }

    for (DirectoryEntry& dirent : entries) {
        if (strcmp(dirent.name, ent->name) == 0) {
            Log::Error("[Ext2] Link: Directory entry %s already exists!", ent->name);
            return -EEXIST;
        }
    }

    entries.add_back(*ent);

    file->nlink++;
    file->e2inode.linkCount++;

    SyncNode(file);

    return WriteDir(node, entries);
}

int Ext2::Ext2Volume::Unlink(Ext2Node* node, DirectoryEntry* ent, bool unlinkDirectories) {
    List<DirectoryEntry> entries;
    if (int e = ListDir(node, entries)) {
        Log::Error("[Ext2] Unlink: Error listing directory!", ent->inode);
        return e;
    }

    for (unsigned i = 0; i < entries.get_length(); i++) {
        if (strcmp(entries[i].name, ent->name) == 0) {
            ent->inode = entries.remove_at(i).inode;
            goto found;
        }
    }

    Log::Error("[Ext2] Unlink: Directory entry %s does not exist!", ent->name);
    return -ENOENT;

found:
    if (!ent->inode) {
        Log::Error("[Ext2] Unlink: Invalid inode %d", ent->inode);
        return -EINVAL;
    }

    {
        ScopedSpinLock lockInodes(m_inodesLock);
        if (Ext2Node * file; inodeCache.get(ent->inode, file)) {
            if ((file->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY) {
                if (!unlinkDirectories) {
                    return -EISDIR;
                }
            }

            file->nlink--;
            file->e2inode.linkCount--;

            if (!file->handleCount) {
                inodeCache.remove(file->inode);
                CleanNode(file);
            }
        } else {
            ext2_inode_t e2inode;
            if (int e = ReadInode(ent->inode, e2inode)) {
                IF_DEBUG(debugLevelExt2 >= DebugLevelNormal,
                        { Log::Error("[Ext2] Link: Error reading inode %d", ent->inode); });
                return e;
            }

            if ((e2inode.mode & EXT2_S_IFMT) == EXT2_S_IFDIR) {
                if (!unlinkDirectories) {
                    return -EISDIR;
                }
            }

            e2inode.linkCount--;

            if (e2inode.linkCount) {
                SyncInode(e2inode, ent->inode);
            } else { // Last link, delete inode
                EraseInode(e2inode, ent->inode);
            }
        }
    }

    return WriteDir(node, entries);
}

int Ext2::Ext2Volume::Truncate(Ext2Node* node, off_t length) {
    if (length < 0) {
        return -EINVAL;
    }

    if (length > node->e2inode.blockCount * 512) { // We need to allocate blocks
        uint64_t blocksNeeded = (length + blocksize - 1) / blocksize;
        uint64_t blocksAllocated = node->e2inode.blockCount / (blocksize / 512);

        while (blocksAllocated < blocksNeeded) {
            SetInodeBlock(blocksAllocated++, node->e2inode, AllocateBlock());
        }

        node->e2inode.blockCount = blocksNeeded * (blocksize / 512);
    }

    node->size = node->e2inode.size = length; // TODO: Actually free blocks in inode if possible

    SyncNode(node);

    return 0;
}

void Ext2::Ext2Volume::CleanNode(Ext2Node* node) {
    if (node->handleCount > 0) {
        Log::Warning("[Ext2] CleanNode: Node (inode %d) is referenced by %d handles", node->inode, node->handleCount);
        return;
    }

    if (node->e2inode.linkCount == 0) { // No links to file
        EraseInode(node->e2inode, node->inode);
    }

    inodeCache.remove(node->inode);
    delete node;
}

Ext2::Ext2Node::Ext2Node(Ext2Volume* vol, ext2_inode_t& ino, ino_t inode) {
    this->vol = vol;
    volumeID = vol->volumeID;

    uid = ino.uid;
    size = ino.size;
    nlink = ino.linkCount;
    this->inode = inode;

    switch (ino.mode & EXT2_S_IFMT) {
    case EXT2_S_IFBLK:
        flags = FS_NODE_BLKDEVICE;
        break;
    case EXT2_S_IFCHR:
        flags = FS_NODE_CHARDEVICE;
        break;
    case EXT2_S_IFDIR:
        flags = FS_NODE_DIRECTORY;
        break;
    case EXT2_S_IFLNK:
        flags = FS_NODE_SYMLINK;
        break;
    case EXT2_S_IFSOCK:
        flags = FS_NODE_SOCKET;
        break;
    default:
        flags = FS_NODE_FILE;
        break;
    }

    e2inode = ino;
}

int Ext2::Ext2Node::ReadDir(DirectoryEntry* ent, uint32_t idx) {
    flock.AcquireRead();
    auto ret = vol->ReadDir(this, ent, idx);
    flock.ReleaseRead();
    return ret;
}

FsNode* Ext2::Ext2Node::FindDir(const char* name) {
    flock.AcquireRead();
    auto ret = vol->FindDir(this, name);
    flock.ReleaseRead();
    return ret;
}

ssize_t Ext2::Ext2Node::Read(size_t offset, size_t size, uint8_t* buffer) {
    flock.AcquireRead();
    auto ret = vol->Read(this, offset, size, buffer);
    flock.ReleaseRead();
    return ret;
}

ssize_t Ext2::Ext2Node::Write(size_t offset, size_t size, uint8_t* buffer) {
    flock.AcquireWrite();
    auto ret = vol->Write(this, offset, size, buffer);
    flock.ReleaseWrite();
    return ret;
}

int Ext2::Ext2Node::Create(DirectoryEntry* ent, uint32_t mode) {
    flock.AcquireWrite();
    auto ret = vol->Create(this, ent, mode);
    flock.ReleaseWrite();
    return ret;
}

int Ext2::Ext2Node::CreateDirectory(DirectoryEntry* ent, uint32_t mode) {
    flock.AcquireWrite();
    auto ret = vol->CreateDirectory(this, ent, mode);
    flock.ReleaseWrite();
    return ret;
}

ssize_t Ext2::Ext2Node::ReadLink(char* pathBuffer, size_t bufSize) {
    flock.AcquireRead();
    auto ret = vol->ReadLink(this, pathBuffer, bufSize);
    flock.ReleaseRead();
    return ret;
}

int Ext2::Ext2Node::Link(FsNode* n, DirectoryEntry* d) {
    if (!n->inode) {
        Log::Warning("[Ext2] Ext2::Ext2Node::Link: Invalid inode %d", n->inode);
        return -EINVAL;
    }

    flock.AcquireWrite();
    auto ret = vol->Link(this, (Ext2Node*)n, d);
    flock.ReleaseWrite();
    return ret;
}

int Ext2::Ext2Node::Unlink(DirectoryEntry* d, bool unlinkDirectories) {
    flock.AcquireWrite();
    auto ret = vol->Unlink(this, d, unlinkDirectories);
    flock.ReleaseWrite();
    return ret;
}

int Ext2::Ext2Node::Truncate(off_t length) {
    flock.AcquireWrite();
    auto ret = vol->Truncate(this, length);
    flock.ReleaseWrite();
    return ret;
}

void Ext2::Ext2Node::Sync() { vol->SyncNode(this); }

void Ext2::Ext2Node::Close() {
    handleCount--;

    if (handleCount == 0) {
        vol->CleanNode(this); // Remove from the cache
        return;
    }
}
} // namespace fs