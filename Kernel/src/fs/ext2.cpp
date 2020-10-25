#include <fs/ext2.h>

#include <logging.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
 
#ifdef EXT2_ENABLE_TIMER
    #include <timer.h>
#endif

namespace fs::Ext2{
    int Identify(PartitionDevice* part){
        ext2_superblock_t* superblock = (ext2_superblock_t*)kmalloc(sizeof(ext2_superblock_t));

        if(part->Read(EXT2_SUPERBLOCK_LOCATION / part->parentDisk->blocksize, sizeof(ext2_superblock_t), superblock)){
            return -1; // Disk Error
        }

        int isExt2 = 0;

        if(superblock->magic == EXT2_SUPER_MAGIC){
            isExt2 = 1;
        }

        kfree(superblock);

        return isExt2;
    }

    Ext2Volume::Ext2Volume(PartitionDevice* part, const char* name){
        this->part = part;

        if(part->Read(EXT2_SUPERBLOCK_LOCATION / part->parentDisk->blocksize, sizeof(ext2_superblock_t), &super)){
            Log::Error("[Ext2] Disk Error Initializing Volume");
            error = DiskReadError;
            return; // Disk Error
        }

        if(super.revLevel){ // If revision level >= 0 grab the extended superblock as well
            if(part->Read(EXT2_SUPERBLOCK_LOCATION / part->parentDisk->blocksize, sizeof(ext2_superblock_t) + sizeof(ext2_superblock_extended_t), &super)){
                Log::Error("[Ext2] Disk Error Initializing Volume");
                error = DiskReadError;
                return; // Disk Error
            }

            if((superext.featuresIncompat & (~EXT2_INCOMPAT_FEATURE_SUPPORT)) != 0 || (superext.featuresRoCompat & (~EXT2_READONLY_FEATURE_SUPPORT)) != 0){ // Check support for incompatible/read-only compatible features
                Log::Error("[Ext2] Incompatible Ext2 features present (Incompt: %x, Read-only Compt: %x). Will not mount volume.", superext.featuresIncompat, superext.featuresRoCompat);
                error = IncompatibleError;
                return;
            }

            if(superext.featuresIncompat & IncompatibleFeatures::Filetype) filetype = true;
            else filetype = false;

            if(superext.featuresRoCompat & ReadonlyFeatures::LargeFiles) largeFiles = true;
            else largeFiles = false;

            if(superext.featuresRoCompat & ReadonlyFeatures::Sparse) sparse = true;
            else sparse = false;
        } else {
            memset(&superext, 0, sizeof(ext2_superblock_extended_t));
        }

        blockGroupCount = (super.blockCount % super.blocksPerGroup) ? (super.blockCount / super.blocksPerGroup + 1) : (super.blockCount / super.blocksPerGroup); // Round up
        uint32_t secondBlockGroupCount = (super.inodeCount % super.inodesPerGroup) ? (super.inodeCount / super.inodesPerGroup + 1) : (super.inodeCount / super.inodesPerGroup); // Round up

        if(blockGroupCount != secondBlockGroupCount){
            Log::Error("[Ext2] Block group count mismatch.");
            error = MiscError;
            return;
        }

        blocksize = 1024U << super.logBlockSize;
        superBlockIndex = LocationToBlock(EXT2_SUPERBLOCK_LOCATION);

        if(super.revLevel){
            inodeSize = superext.inodeSize;
        } else {
            inodeSize = 128;
        }

        Log::Info("[Ext2] Initializing Volume\tRevision: %d, Block Size: %d, %d KB/%d KB used, Last mounted on: %s", super.revLevel, blocksize, super.freeBlockCount * blocksize / 1024, super.blockCount * blocksize / 1024, superext.lastMounted);
        Log::Info("[Ext2] Block Group Count: %d, Inodes Per Block Group: %d, Inode Size: %d", blockGroupCount, super.inodesPerGroup, inodeSize);
        Log::Info("[Ext2] Sparse Superblock? %s Large Files? %s, Filetype Extension? %s", (sparse ? "Yes" : "No"), (largeFiles ? "Yes" : "No"), (filetype ? "Yes" : "No"));

        blockGroups = (ext2_blockgrp_desc_t*)kmalloc(blockGroupCount * sizeof(ext2_blockgrp_desc_t));
        
        uint64_t blockGLBA = BlockToLBA(LocationToBlock(EXT2_SUPERBLOCK_LOCATION) + 1); // One block from the superblock
        
        if(part->Read(blockGLBA, blockGroupCount * sizeof(ext2_blockgrp_desc_t), blockGroups)){
            Log::Error("[Ext2] Disk Error Initializing Volume");
            error = DiskReadError;
            return; // Disk Error
        }

        ext2_inode_t root;
        if(ReadInode(EXT2_ROOT_INODE_INDEX, root)){
            Log::Error("[Ext2] Disk Error Initializing Volume");
            error = DiskReadError;
            return;
        }

        Ext2Node* e2mountPoint = new Ext2Node(this, root, EXT2_ROOT_INODE_INDEX);
        mountPoint = e2mountPoint;

        mountPointDirent.node = mountPoint; 
        strcpy(mountPointDirent.name, name);
    }

    void Ext2Volume::WriteSuperblock(){
        uint8_t buffer[blocksize];

        uint32_t superindex = LocationToBlock(EXT2_SUPERBLOCK_LOCATION);
        if(ReadBlockCached(superindex, buffer)){
            Log::Info("[Ext2] WriteBlock: Error reading block %d", superindex);
            return;
        }

        memcpy(buffer + (EXT2_SUPERBLOCK_LOCATION % blocksize), &super, sizeof(ext2_superblock_t) + sizeof(ext2_superblock_extended_t));
        
        if(WriteBlockCached(superindex, buffer)){
            Log::Info("[Ext2] WriteBlock: Error writing block %d", superindex);
            return;
        }
    }

    void Ext2Volume::WriteBlockGroupDescriptor(uint32_t index){
        uint32_t firstBlockGroup = LocationToBlock(EXT2_SUPERBLOCK_LOCATION) + 1;
        uint32_t block = firstBlockGroup + LocationToBlock(index * sizeof(ext2_blockgrp_desc_t));
        uint8_t buffer[blocksize];
        
        if(ReadBlockCached(block, buffer)){
            Log::Info("[Ext2] WriteBlock: Error reading block %d", block);
            return;
        }

        memcpy(buffer + ((index * sizeof(ext2_blockgrp_desc_t)) % blocksize), &blockGroups[index], sizeof(ext2_blockgrp_desc_t));
        
        if(WriteBlock(block, buffer)){
            Log::Info("[Ext2] WriteBlock: Error writing block %d", block);
            return;
        }
    }

    uint32_t Ext2Volume::GetInodeBlock(uint32_t index, ext2_inode_t& ino){
        uint32_t blocksPerPointer = blocksize / sizeof(uint32_t); // Amount of blocks in a indirect block table
        uint32_t singlyIndirectStart = EXT2_DIRECT_BLOCK_COUNT;
        uint32_t doublyIndirectStart = singlyIndirectStart + blocksPerPointer;
        uint32_t triplyIndirectStart = doublyIndirectStart + blocksPerPointer * blocksPerPointer;

        assert(index < triplyIndirectStart + blocksPerPointer * blocksPerPointer * blocksPerPointer); // Make sure that an invalid index was not passed

        if(index < EXT2_DIRECT_BLOCK_COUNT){
            // Index lies within the direct blocklist
            return ino.blocks[index];
        } else if(index < doublyIndirectStart){
            // Index lies within the singly indirect blocklist
            uint32_t buffer[blocksize / sizeof(uint32_t)];

            if(int e = ReadBlockCached(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)){
                (void)e;
                error = DiskReadError;
                return 0;
            }

            return buffer[index - singlyIndirectStart];
        } else if(index < triplyIndirectStart){
            // Index lies within the doubly indirect blocklist
            uint32_t blockPointers[blocksize / sizeof(uint32_t)];
            uint32_t buffer[blocksize / sizeof(uint32_t)];

            if(int e = ReadBlockCached(ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX], blockPointers)){
                (void)e;
                error = DiskReadError;
                return 0;
            }

            uint32_t blockPointer = blockPointers[(index - doublyIndirectStart) / blocksPerPointer];

            if(int e = ReadBlockCached(blockPointer, buffer)){
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

    Vector<uint32_t> Ext2Volume::GetInodeBlocks(uint32_t index, uint32_t count, ext2_inode_t& ino){
        uint32_t blocksPerPointer = blocksize / sizeof(uint32_t); // Amount of blocks in a indirect block table
        uint32_t singlyIndirectStart = EXT2_DIRECT_BLOCK_COUNT;
        uint32_t doublyIndirectStart = singlyIndirectStart + blocksPerPointer;
        uint32_t triplyIndirectStart = doublyIndirectStart + blocksPerPointer * blocksPerPointer;

        assert(index + count < triplyIndirectStart + blocksPerPointer * blocksPerPointer * blocksPerPointer); // Make sure that an invalid index was not passed

        unsigned i = index;
        Vector<uint32_t> blocks;
        blocks.reserve(count);

        while(i < EXT2_DIRECT_BLOCK_COUNT && i < index + count){
            // Index lies within the direct blocklist
            blocks.add_back(ino.blocks[i++]);
        }
        
        if(i < doublyIndirectStart && i < index + count){
            // Index lies within the singly indirect blocklist
            uint32_t buffer[blocksize / sizeof(uint32_t)];

            if(int e = ReadBlockCached(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)){
                Log::Info("[Ext2] GetInodeBlocks: Error %i reading block %u (singly indirect block)", e, ino.blocks[EXT2_SINGLY_INDIRECT_INDEX]);
                error = DiskReadError;

                blocks.clear();
                return blocks;
            }

            while(i < doublyIndirectStart && i < index + count){
                uint32_t block = buffer[(i++) - singlyIndirectStart];
                if(block >= super.blockCount){
                    Log::Warning("[Ext2] GetInodeBlocks: Invalid singly indirect block %u, Singly indirect blocklist: %u", block, ino.blocks[EXT2_SINGLY_INDIRECT_INDEX]);
                }
                blocks.add_back(block);
            }
        }
        
        if(i < triplyIndirectStart && i < index + count){
            // Index lies within the doubly indirect blocklist
            uint32_t blockPointers[blocksize / sizeof(uint32_t)];
            uint32_t buffer[blocksize / sizeof(uint32_t)];

            if(int e = ReadBlockCached(ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX], blockPointers)){
                Log::Info("[Ext2] GetInodeBlocks: Error %i reading block %u (doubly indirect block)", e, ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX]);
                error = DiskReadError;

                blocks.clear();
                return blocks;
            }

            while(i < triplyIndirectStart && i < index + count){
                uint32_t blockPointer = blockPointers[(i - doublyIndirectStart) / blocksPerPointer];

                if(int e = ReadBlockCached(blockPointer, buffer)){
                    Log::Info("[Ext2] GetInodeBlocks: Error %i reading block %u (doubly indirect block pointer)", e, blockPointer);
                    error = DiskReadError;

                    blocks.clear();
                    return blocks;
                }

                uint32_t blockPointerEnd = i + (blocksPerPointer - (i - doublyIndirectStart) % blocksPerPointer);
                while(i < blockPointerEnd && i < index + count){
                    uint32_t block = buffer[(i - doublyIndirectStart) % blocksPerPointer];
                    if(block >= super.blockCount){
                        Log::Warning("[Ext2] GetInodeBlocks: Invalid doubly indirect block %u", block);
                    }

                    blocks.add_back(block);
                    i++;
                }
            }
        } 
        
        if(i < index + count) {
            assert(!"[Ext2] Yet to support triply indirect blocklists");

            blocks.clear();
            return blocks;
        }

        return blocks;
    }
    
    void Ext2Volume::SetInodeBlock(uint32_t index, ext2_inode_t& ino, uint32_t block){
        uint32_t blocksPerPointer = blocksize / sizeof(uint32_t); // Amount of blocks in a indirect block table
        uint32_t singlyIndirectStart = EXT2_DIRECT_BLOCK_COUNT;
        uint32_t doublyIndirectStart = singlyIndirectStart + blocksPerPointer;
        uint32_t triplyIndirectStart = doublyIndirectStart + blocksPerPointer * blocksPerPointer;

        assert(index < triplyIndirectStart + blocksPerPointer * blocksPerPointer * blocksPerPointer); // Make sure that an invalid index was not passed

        if(index < EXT2_DIRECT_BLOCK_COUNT){
            // Index lies within the direct blocklist
            ino.blocks[index] = block;
        } else if(index < doublyIndirectStart){
            // Index lies within the singly indirect blocklist
            uint32_t buffer[blocksize / sizeof(uint32_t)];

            if(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX] == 0){
                ino.blocks[EXT2_SINGLY_INDIRECT_INDEX] = AllocateBlock();
            }

            if(int e = ReadBlockCached(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)){
                (void)e;
                error = DiskReadError;
                return;
            }

            buffer[index - singlyIndirectStart] = block;
            
            if(int e = WriteBlockCached(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)){
                (void)e;
                error = DiskWriteError;
                return;
            }
            return;
        } else if(index < triplyIndirectStart){
            // Index lies within the doubly indirect blocklist
            uint32_t blockPointers[blocksize / sizeof(uint32_t)];
            uint32_t buffer[blocksize / sizeof(uint32_t)];

            if(int e = ReadBlockCached(ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX], blockPointers)){ // Read indirect block pointer list
                (void)e;
                error = DiskReadError;
                return;
            }

            uint32_t blockPointer = blockPointers[(index - doublyIndirectStart) / blocksPerPointer];

            if(int e = ReadBlockCached(blockPointer, buffer)){ // Read blocklist
                (void)e;
                error = DiskReadError;
                return;
            }

            buffer[(index - doublyIndirectStart) % blocksPerPointer] = block; // Update the index
            
            if(int e = WriteBlockCached(blockPointer, buffer)){ // Write our updated blocklist
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

    int Ext2Volume::ReadInode(uint32_t num, ext2_inode_t& inode){
        uint8_t buf[part->parentDisk->blocksize];
        uint64_t lba = InodeLBA(num);

        if(int e = part->Read(lba, part->parentDisk->blocksize, buf)){
            Log::Error("[Ext2] Disk Error (%d) Reading Inode %d", e, num);
            error = DiskReadError;
            return e;
        }

        inode = *(ext2_inode_t*)(buf + (ResolveInodeBlockGroupIndex(num) * inodeSize) % part->parentDisk->blocksize);
        return 0;
    }

    int Ext2Volume::ReadBlock(uint32_t block, void* buffer){
        if(block > super.blockCount)
            return 1;

        if(int e = part->Read(BlockToLBA(block), blocksize, buffer)){
            Log::Error("[Ext2] Disk error (%d) reading block %d (blocksize: %d)", e, block, blocksize);
            return e;
        }

        return 0;
    }
    
    int Ext2Volume::WriteBlock(uint32_t block, void* buffer){
        if(block > super.blockCount)
            return 1;

        if(int e = part->Write(BlockToLBA(block), blocksize, buffer)){
            Log::Error("[Ext2] Disk error (%e) reading block %d (blocksize: %d)", e, block, blocksize);
            return e;
        }

        return 0;
    }
    
    int Ext2Volume::ReadBlockCached(uint32_t block, void* buffer){
        if(block > super.blockCount)
            return 1;

        uint8_t* cachedBlock;
        if((cachedBlock = blockCache.get(block))){
            memcpy(buffer, cachedBlock, blocksize);
        } else {
            cachedBlock = (uint8_t*)kmalloc(blocksize);
            if(int e = part->Read(BlockToLBA(block), blocksize, cachedBlock)){
                Log::Error("[Ext2] Disk error (%d) reading block %d (blocksize: %d)", e, block, blocksize);
                return e;
            }
            blockCache.insert(block, cachedBlock);
            blockCacheMemoryUsage += blocksize;
            
            memcpy(buffer, cachedBlock, blocksize);
        }
        return 0;
    }
    
    int Ext2Volume::WriteBlockCached(uint32_t block, void* buffer){
        if(block > super.blockCount)
            return -1;

        uint8_t* cachedBlock;
        if((cachedBlock = blockCache.get(block))){
            memcpy(cachedBlock, buffer, blocksize);
        }
        
        if(int e = part->Write(BlockToLBA(block), blocksize, buffer)){
            Log::Error("[Ext2] Disk error (%d) reading block %d (blocksize: %d)", e, block, blocksize);
            return e;
        }

        return 0;
    }
    
    uint32_t Ext2Volume::AllocateBlock(){
        for(unsigned i = 0; i < blockGroupCount; i++){
            ext2_blockgrp_desc_t& group = blockGroups[i];

            if(group.freeBlockCount <= 0) continue; // No free blocks in this blockgroup

            uint8_t bitmap[blocksize / sizeof(uint8_t)];

            if(uint8_t* cachedBitmap = bitmapCache.get(group.blockBitmap)){
                memcpy(bitmap, cachedBitmap, blocksize);
            } else {
                if(int e = ReadBlock(group.blockBitmap, bitmap)){
                    Log::Error("[Ext2] Disk error (%d) reading block bitmap (group %d)", e, i);
                    error = DiskReadError;
                    return 0;
                }
            }

            uint32_t block = 0;
            unsigned e = 0;
            for(; e < blocksize / sizeof(uint8_t); e++){
                if(bitmap[e] == UINT8_MAX) continue; // Full

                for(uint8_t b = 0; b < sizeof(uint8_t) * 8; b++){ // Iterate through all bits in the entry
                    if(((bitmap[e] >> b) & 0x1) == 0){
                        bitmap[e] |= (1U << b);
                        block = (i * super.blocksPerGroup) + e * (sizeof(uint8_t) * 8) + b; // Block Number = (Group number * blocks per group) + (bitmap entry * 8 (8 bits per byte)) + bit (block 0 is least significant bit, block 7 is most significant)
                        break;
                    }
                }


                if(block) break;
            }

            if(!block){
                continue;
            }

            if(uint8_t* cachedBitmap = bitmapCache.get(group.blockBitmap)){
                memcpy(cachedBitmap, bitmap, blocksize);
            }

            if(int e = WriteBlock(group.blockBitmap, bitmap)){
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
     
    int Ext2Volume::FreeBlock(uint32_t block){
        if(block > super.blockCount) return -1;

        ext2_blockgrp_desc_t& group = blockGroups[block / super.blocksPerGroup];

        uint8_t bitmap[blocksize / sizeof(uint8_t)];

        if(uint8_t* cachedBitmap = bitmapCache.get(group.blockBitmap)){
            memcpy(bitmap, cachedBitmap, blocksize);
        } else {
            if(int e = ReadBlock(group.blockBitmap, bitmap)){
                Log::Error("[Ext2] Disk error (%d) reading block bitmap (group %d)", e, block / super.blocksPerGroup);
                error = DiskReadError;
                return -1;
            }
        }

        int bmapIndex = (block % super.blocksPerGroup) / (sizeof(uint8_t) * 8);
        int bitmask = ~(1 << (block % 8));
        bitmap[bmapIndex] &= bitmask;

        if(uint8_t* cachedBitmap = bitmapCache.get(group.blockBitmap)){
            memcpy(cachedBitmap, bitmap, blocksize);
        }

        if(int e = WriteBlock(group.blockBitmap, bitmap)){
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
    
    Ext2Node* Ext2Volume::CreateNode(){
        for(unsigned i = 0; i < blockGroupCount; i++){
            ext2_blockgrp_desc_t& group = blockGroups[i];

            if(group.freeInodeCount <= 0) continue; // No free inodes in this blockgroup

            uint8_t bitmap[blocksize / sizeof(uint8_t)];

            if(int e = ReadBlockCached(group.inodeBitmap, bitmap)){
                Log::Error("[Ext2] Disk error (%d) reading inode bitmap (group %d)", e, i);
                error = DiskReadError;
                return nullptr;
            }

            uint32_t inode = 0;

            for(unsigned e = 0; e < blocksize / sizeof(uint8_t); e++){
                if(bitmap[e] == UINT8_MAX) continue; // Full

                for(uint8_t b = 0; b < sizeof(uint8_t) * 8; b++){ // Iterate through all bits in the entry
                    if(((bitmap[e] >> b) & 0x1) == 0){
                        bitmap[e] |= (1U << b);
                        inode = (i * super.inodesPerGroup) + e * (sizeof(uint8_t) * 8) + b + 1; // Inode Number = (Group number * inodes per group) + (bitmap entry * 8 (8 bits per byte)) + bit (inode 1 is least significant bit, block 8 is most significant) + 1 (inodes start at 1)
                        
                        break;
                    }
                }

                if(inode) break;
            }

            if(!inode) continue;

            if(int e = WriteBlockCached(group.inodeBitmap, bitmap)){
                Log::Error("[Ext2] Disk error (%d) write inode bitmap (group %d)", e, i);
                error = DiskWriteError;
                return nullptr;
            }

            ext2_inode_t ino;

            memset(&ino, 0, sizeof(ext2_inode_t));

            ino.blocks[0] = AllocateBlock(); // Give it one block
            ino.uid = 0;
            ino.mode = 0;
            ino.accessTime = ino.createTime = ino.deleteTime = ino.modTime = 0;
            ino.gid = 0;
            ino.blockCount = blocksize / 512;
            ino.linkCount = 0;
            ino.size = ino.sizeHigh = 0;
            ino.fragAddr = 0;
            ino.fileACL = 0;

            super.freeInodeCount--;
            blockGroups[i].freeInodeCount--;

            Ext2Node* node = new Ext2Node(this, ino, inode);
            WriteSuperblock();
            WriteBlockGroupDescriptor(i);

            //Log::Info("[Ext2] Created inode %d", node->inode);

            return node;
        }

        Log::Error("[Ext2] No inodes left on the filesystem!");
        return nullptr;
    }
    
    int Ext2Volume::EraseInode(ext2_inode_t& e2inode, uint32_t inode){
        if(inode > super.inodeCount) return -1;

        if(e2inode.linkCount > 0){
            Log::Warning("[Ext2] EraseInode: inode %d contains %d links! Will not erase.", inode, e2inode.linkCount);
            return -2;
        }

        for(unsigned i = 0; i < e2inode.blockCount * (blocksize / 512); i++){
            uint32_t block = GetInodeBlock(i, e2inode);
            FreeBlock(block);
            if(blockCache.get(block)){
                blockCache.remove(block);
            }
        }
        
        if(e2inode.blocks[EXT2_SINGLY_INDIRECT_INDEX]){
            FreeBlock(e2inode.blocks[EXT2_SINGLY_INDIRECT_INDEX]);
            if(e2inode.blocks[EXT2_DOUBLY_INDIRECT_INDEX]){
                uint32_t blockPointers[blocksize / sizeof(uint32_t)];

                if(int e = ReadBlockCached(e2inode.blocks[EXT2_DOUBLY_INDIRECT_INDEX], blockPointers)){
                    (void)e;
                    error = DiskReadError;
                    return 0;
                }
                
                for(unsigned i = 0; i < (blocksize / sizeof(uint32_t)) && blockPointers[i] != 0; i++){
                    FreeBlock(blockPointers[i]);
                    if(blockCache.get(blockPointers[i])){
                        blockCache.remove(blockPointers[i]);
                    }
                }

                FreeBlock(e2inode.blocks[EXT2_DOUBLY_INDIRECT_INDEX]);

                if(e2inode.blocks[EXT2_TRIPLY_INDIRECT_INDEX]){
                    Log::Error("[Ext2] We do not support triply indirect blocks, will not free them.");
                }
            }
        }

        ext2_blockgrp_desc_t& group = blockGroups[inode / super.inodesPerGroup];

        uint8_t bitmap[blocksize / sizeof(uint8_t)];

        if(uint8_t* cachedBitmap = bitmapCache.get(group.inodeBitmap)){
            memcpy(bitmap, cachedBitmap, blocksize);
        } else {
            if(int e = ReadBlock(group.inodeBitmap, bitmap)){
                Log::Error("[Ext2] Disk error (%d) reading inode bitmap (group %d)", e, inode / super.inodesPerGroup);
                error = DiskReadError;
                return -1;
            }
        }

        int bmapIndex = (inode % super.inodesPerGroup) / (sizeof(uint8_t) * 8);
        int bitmask = ~(1 << (inode % 8));
        bitmap[bmapIndex] &= bitmask;

        if(uint8_t* cachedBitmap = bitmapCache.get(group.inodeBitmap)){
            memcpy(cachedBitmap, bitmap, blocksize);
        }

        if(int e = WriteBlock(group.blockBitmap, bitmap)){
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
    
    int Ext2Volume::ListDir(Ext2Node* node, List<DirectoryEntry>& entries){
        if((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
            Log::Warning("[Ext2] ListDir: Not a directory (inode %d)", node->inode);
            error = MiscError;
            return -1;
        }

        if(node->inode < 1){
            Log::Warning("[Ext2] ReadDir: Invalid inode: %d", node->inode);
            error = InvalidInodeError;
            return -1;
        }

        ext2_inode_t& ino = node->e2inode;

        uint8_t* buffer = (uint8_t*)kmalloc(blocksize);
        uint32_t currentBlockIndex = 0;
        uint32_t blockOffset = 0;
        uint32_t totalOffset = 0;

        ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)buffer;

        if(ReadBlock(GetInodeBlock(currentBlockIndex, ino), buffer)){
            Log::Info("[Ext2] ListDir: Error reading block %d", ino.blocks[currentBlockIndex]);
            error = DiskReadError;
            return -1;
        }

        for(;;){
            if(e2dirent->recordLength == 0) break;

            DirectoryEntry dirent;
            dirent.flags = e2dirent->fileType;
            strncpy(dirent.name, e2dirent->name, e2dirent->nameLength);
            dirent.name[e2dirent->nameLength] = 0;
            dirent.inode = e2dirent->inode;

            entries.add_back(dirent);

            blockOffset += e2dirent->recordLength;
            totalOffset += e2dirent->recordLength;

            if(blockOffset >= blocksize){
                currentBlockIndex++;

                if(currentBlockIndex > ino.blockCount / (blocksize / 512)){
                    // End of dir
                    break;
                }

                blockOffset = 0;
                if(ReadBlock(GetInodeBlock(currentBlockIndex, ino), buffer)){
                    error = DiskReadError;
                    return -1;
                }
            }

            e2dirent = (ext2_directory_entry_t*)(buffer + blockOffset);
        }

        return 0;
    }

    int Ext2Volume::WriteDir(Ext2Node* node, List<DirectoryEntry>& entries){
        if((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
            return -ENOTDIR;
        }

        if(node->inode < 1){
            Log::Warning("[Ext2] WriteDir: Invalid inode: %d", node->inode);
            return -1;
        }

        ext2_inode_t& ino = node->e2inode;

        uint8_t* buffer = (uint8_t*)kmalloc(blocksize);
        uint32_t currentBlockIndex = 0;
        uint32_t blockOffset = 0;
        uint32_t totalOffset = 0;

        ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)kmalloc(sizeof(ext2_directory_entry_t) + NAME_MAX);

        unsigned lastIndex = entries.get_length() - 1;
        for(unsigned i = 0; i < entries.get_length(); i++){
            DirectoryEntry ent = entries[i];

            //Log::Info("[Ext2] Writing entry: %s", ent.name);

            e2dirent->fileType = ent.flags;
            e2dirent->inode = ent.inode;
            strncpy(e2dirent->name, ent.name, strlen(ent.name));
            e2dirent->nameLength = strlen(ent.name);
            
            if(i == lastIndex){
                e2dirent->recordLength = blocksize - blockOffset;
            } else {
                if(strlen(entries[i + 1].name) + sizeof(ext2_directory_entry_t) + blockOffset > blocksize){
                    e2dirent->recordLength = blocksize - blockOffset; // Directory entries cannot span multiple blocks
                } else {
                    e2dirent->recordLength = sizeof(ext2_directory_entry_t) + e2dirent->nameLength;
                    if(e2dirent->recordLength && e2dirent->recordLength % 4){
                        memset(((uint8_t*)e2dirent) + e2dirent->recordLength, 0, 4 - (e2dirent->recordLength % 4)); // Pad with zeros
                        e2dirent->recordLength += 4 - (e2dirent->recordLength % 4); // Round up to nearest multiple of 4
                    }
                }
            }

            memcpy((buffer + blockOffset), e2dirent, sizeof(ext2_directory_entry_t) + e2dirent->nameLength);
            blockOffset += e2dirent->recordLength;
            totalOffset += e2dirent->recordLength;

            if(blockOffset >= blocksize){
                if(WriteBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)){
                    Log::Error("[Ext2] WriteDir: Failed to write directory block");
                    error = DiskWriteError;
                    return -1;
                }

                currentBlockIndex++;

                if(currentBlockIndex > ino.blockCount / (blocksize / 512)){
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

        kfree(buffer);
        kfree(e2dirent);

        SyncNode(node);

        return 0;
    }

    int Ext2Volume::InsertDir(Ext2Node* node, List<DirectoryEntry>& newEntries){
        List<DirectoryEntry> entries;
        ListDir(node, entries);


        for(DirectoryEntry& newEnt : newEntries){
            for(DirectoryEntry& ent : entries){
                if(strcmp(ent.name, newEnt.name) == 0){
                    Log::Info("[Ext2] InsertDir: Entry %s already exists!", newEnt.name);
                    return -EEXIST;
                }
            }
            entries.add_back(newEnt);
        }

        return WriteDir(node, entries);
    }
    
    int Ext2Volume::InsertDir(Ext2Node* node, DirectoryEntry& ent){
        List<DirectoryEntry> ents;
        ents.add_back(ent);

        return InsertDir(node, ents);
    }

    int Ext2Volume::ReadDir(Ext2Node* node, DirectoryEntry* dirent, uint32_t index){
        if((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
            return -ENOTDIR;
        }

        if(node->inode < 1){
            Log::Warning("[Ext2] ReadDir: Invalid inode: %d", node->inode);
            return -1;
        }

        ext2_inode_t& ino = node->e2inode;

        uint8_t* buffer = (uint8_t*)kmalloc(blocksize);
        uint32_t currentBlockIndex = 0;
        uint32_t blockOffset = 0;
        uint32_t totalOffset = 0;

        ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)buffer;

        if(ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)){
            Log::Info("[Ext2] Failed to read block %d", GetInodeBlock(currentBlockIndex, ino));
            error = DiskReadError;
            return -1;
        }

        for(unsigned i = 0; i < index; i++){
            if(e2dirent->recordLength == 0) return 0;

            blockOffset += e2dirent->recordLength;
            totalOffset += e2dirent->recordLength;

            if(blockOffset >= blocksize){
                currentBlockIndex++;

                if(currentBlockIndex >= ino.blockCount / (blocksize / 512)){
                    // End of dir
                    return 0;
                }

                blockOffset = 0;
                if(ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)){
                    Log::Info("[Ext2] Failed to read block");
                    return -1;
                }
            }

            e2dirent = (ext2_directory_entry_t*)(buffer + blockOffset);
        }

        strncpy(dirent->name, e2dirent->name, e2dirent->nameLength);
        dirent->name[e2dirent->nameLength] = 0; // Null terminate
        dirent->flags = e2dirent->fileType;

        return 1;
    }

    FsNode* Ext2Volume::FindDir(Ext2Node* node, char* name){
        if((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
            return nullptr;
        }

        if(node->inode < 1){
            Log::Warning("[Ext2] ReadDir: Invalid inode: %d", node->inode);
            return nullptr;
        }

        ext2_inode_t& ino = node->e2inode;

        uint8_t* buffer = (uint8_t*)kmalloc(blocksize);
        memset(buffer, 0, blocksize);
        uint32_t currentBlockIndex = 0;
        uint32_t blockOffset = 0;
        uint32_t totalOffset = 0;

        ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)buffer;

        if(ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)){
            Log::Info("[Ext2] Failed to read block %d", GetInodeBlock(currentBlockIndex, ino));
            return nullptr;
        }

        while(totalOffset < ino.size && currentBlockIndex < ino.blockCount / (blocksize / 512)){
            /*char buf[e2dirent->nameLength + 1];
            strncpy(buf, e2dirent->name, e2dirent->nameLength);
            buf[e2dirent->nameLength] = 0;
            Log::Info("Checking name %s, inode %d, len %d (parent inode: %d)", buf, e2dirent->inode, e2dirent->recordLength, node->inode);*/

            if(strlen(name) == e2dirent->nameLength && strncmp(name, e2dirent->name, e2dirent->nameLength) == 0){
                break;
            }

            blockOffset += e2dirent->recordLength;
            totalOffset += e2dirent->recordLength;

            if(totalOffset > ino.size) return nullptr;

            if(blockOffset >= blocksize){
                currentBlockIndex++;
                
                if(currentBlockIndex >= ino.blockCount / (blocksize / 512)){
                    // End of dir
                    kfree(buffer);
                    return nullptr;
                }

                blockOffset = 0;

                if(ReadBlockCached(GetInodeBlock(currentBlockIndex, ino), buffer)){
                    Log::Info("[Ext2] Failed to read block");
                    return nullptr;
                }
            }

            e2dirent = (ext2_directory_entry_t*)(buffer + blockOffset);
        }

        if(strlen(name) != e2dirent->nameLength || strncmp(e2dirent->name, name, e2dirent->nameLength) != 0){
            // Not found
            kfree(buffer);
            return nullptr;
        }

        if(!e2dirent->inode || e2dirent->inode > super.inodeCount){
            Log::Error("[Ext2] Directory Entry %s contains invalid inode %d", name, e2dirent->inode);
            kfree(buffer);
            return nullptr;
        }

        Ext2Node* returnNode = inodeCache.get(e2dirent->inode);

        if(!returnNode){ // Could not locate inode in cache
            ext2_inode_t direntInode;
            if(ReadInode(e2dirent->inode, direntInode)){
                Log::Error("[Ext2] Failed to read inode of directory (inode %d) entry %s", node->inode, e2dirent->name);
                return nullptr; // Could not read inode
            }

            returnNode = new Ext2Node(this, direntInode, e2dirent->inode);

            inodeCache.insert(e2dirent->inode, returnNode);
        }

        kfree(buffer);
        return returnNode;
    }

    ssize_t Ext2Volume::Read(Ext2Node* node, size_t offset, size_t size, uint8_t *buffer){
        if(offset > node->size) return -1;
        if(offset + size > node->size) size = node->size - offset;

        uint32_t blockIndex = LocationToBlock(offset);
        uint32_t blockLimit = LocationToBlock(offset + size);
        uint8_t blockBuffer[blocksize];

        //Log::Info("[Ext2] Reading: Block index: %d, Block limit: %d, Offset: %d, Size: %d", blockIndex, blockLimit, offset, size);

        #ifdef EXT2_ENABLE_TIMER
        timeval_t blktv1 = Timer::GetSystemUptimeStruct();
        #endif

        ssize_t ret = size;
        Vector<uint32_t> blocks = GetInodeBlocks(blockIndex, blockLimit - blockIndex + 1, node->e2inode);
        
        #ifdef EXT2_ENABLE_TIMER
        timeval_t blktv2 = Timer::GetSystemUptimeStruct();
        timeval_t readtv1 = Timer::GetSystemUptimeStruct();
        #endif

        for(uint32_t block : blocks){
            if(size <= 0) break;
            
            if(offset && offset % blocksize){
                if(int e = ReadBlockCached(block, blockBuffer); e){
                    Log::Info("[Ext2] Error %i reading block %u", e, block);
                    error = DiskReadError;
                    //return -1;
                }

                size_t readSize = blocksize - (offset % blocksize);
                size_t readOffset = (offset % blocksize);

                if(readSize > size) readSize = size;

                memcpy(buffer, blockBuffer + readOffset, readSize);

                size -= readSize;
                buffer += readSize;
                offset += readSize;
            } else if(size >= blocksize){
                if(int e = ReadBlockCached(block, buffer); e){
                    Log::Info("[Ext2] Error %i reading block %u", e, block);
                    error = DiskReadError;
                    //return -1;
                }
                size -= blocksize;
                buffer += blocksize;
                offset += blocksize;
            } else {
                if(int e = ReadBlockCached(block, blockBuffer); e){
                    Log::Info("[Ext2] Error %i reading block %u", e, block);
                    error = DiskReadError;
                    //return -1;
                }
                memcpy(buffer, blockBuffer, size);
                size = 0;
                break;
            }
        }

        #ifdef EXT2_ENABLE_TIMER
        timeval_t readtv2 = Timer::GetSystemUptimeStruct();

        Log::Info("[Ext2] Retrieving inode blocks took %d ms, Reading inode blocks took %d ms", Timer::TimeDifference(blktv2, blktv1), Timer::TimeDifference(readtv2, readtv1));
        #endif

        if(size){
            Log::Info("[Ext2] Requested %d bytes, read %d bytes (offset: %d)", ret, ret - size, offset);
            return ret - size;
        }

        return ret;
    }

    ssize_t Ext2Volume::Write(Ext2Node* node, size_t offset, size_t size, uint8_t *buffer){
        if(readOnly) {
            error = FilesystemAccessError;
            return -EROFS;
        }

        uint32_t blockIndex = LocationToBlock(offset); // Index of first block to write
        uint32_t fileBlockCount = node->e2inode.blockCount / (blocksize / 512); // Size of file in blocks
        uint32_t blockLimit = LocationToBlock(offset + size); // Amount of blocks to write
        uint8_t* blockBuffer = (uint8_t*)kmalloc(blocksize); // block buffer
        bool sync = false; // Need to sync the inode?

        if(blockLimit >= fileBlockCount){
            //Log::Info("[Ext2] Allocating blocks for inode %d", node->inode);
            for(unsigned i = fileBlockCount; i <= blockLimit; i++){
                uint32_t block = AllocateBlock();
                SetInodeBlock(i, node->e2inode, block);
            }
            node->e2inode.blockCount = (blockLimit + 1) * (blocksize / 512);

            WriteSuperblock();
            sync = true;
        }

        if(offset + size > node->size){
            node->size = offset + size;
            node->e2inode.size = node->size;

            sync = true;
        }

        if(sync){
            SyncNode(node);
        }

        //Log::Info("[Ext2] Writing: Block index: %d, Blockcount: %d, Offset: %d, Size: %d", blockIndex, blockLimit - blockIndex + 1, offset, size);

        size_t ret = size;

        for(; blockIndex <= blockLimit && size > 0; blockIndex++){
            uint32_t block = GetInodeBlock(blockIndex, node->e2inode);
            
            if(offset % blocksize){
                ReadBlockCached(block, blockBuffer);

                size_t writeSize = blocksize - (offset % blocksize);
                size_t writeOffset = (offset % blocksize);
                
                if(writeSize > size) writeSize = size;

                memcpy(blockBuffer + writeOffset, buffer, writeSize);
                WriteBlockCached(block, blockBuffer);

                size -= writeSize;
                buffer += writeSize;
                offset += writeSize;
            } else if(size >= blocksize){
                memcpy(blockBuffer, buffer, blocksize);
                WriteBlockCached(block, blockBuffer);

                size -= blocksize;
                buffer += blocksize;
                offset += blocksize;
            } else {
                ReadBlockCached(block, blockBuffer);
                memcpy(blockBuffer, buffer, size);
                WriteBlockCached(block, blockBuffer);

                buffer += size;
                offset += size;
                size = 0;
                break;
            }
        }

        if(size > 0){
            ret -= size;
        }

        return ret;
    }

    void Ext2Volume::SyncInode(ext2_inode_t& e2inode, uint32_t inode){
        uint8_t buf[part->parentDisk->blocksize];
        uint64_t lba = InodeLBA(inode);

        if(int e = part->Read(lba, part->parentDisk->blocksize, buf)){
            Log::Error("[Ext2] Sync: Disk Error (%d) Reading Inode %d", e, inode);
            error = DiskReadError;
            return;
        }

        *(ext2_inode_t*)(buf + (ResolveInodeBlockGroupIndex(inode) * inodeSize) % part->parentDisk->blocksize) = e2inode;

        if(int e = part->Write(lba, part->parentDisk->blocksize, buf)){
            Log::Error("[Ext2] Sync: Disk Error (%d) Writing Inode %d", e, inode);
            error = DiskWriteError;
            return;
        }
    }

    void Ext2Volume::SyncNode(Ext2Node* node){
        SyncInode(node->e2inode, node->inode);
    }

    int Ext2Volume::Create(Ext2Node* node, DirectoryEntry* ent, uint32_t mode){
        if((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) return -ENOTDIR;

        if(FindDir(node, ent->name)){
            Log::Info("[Ext2] Create: Entry %s already exists!", ent->name);
            return -EEXIST;
        }

        Ext2Node* file = CreateNode();
        
        if(!file){
            return -1;
        }

        file->e2inode.mode = EXT2_S_IFREG;
        file->flags = FS_NODE_FILE;
        file->nlink = 1;
        file->e2inode.linkCount = 1;
        inodeCache.insert(file->inode, file);
        ent->node = file;
        ent->inode = file->inode;
        ent->flags = EXT2_FT_REG_FILE;

        SyncNode(file);
        return InsertDir(node, *ent);
    }

    int Ext2Volume::CreateDirectory(Ext2Node* node, DirectoryEntry* ent, uint32_t mode){
        if(readOnly){
            return -EROFS;
        }

        if((node->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY) {
            Log::Info("[Ext2] Not a directory!");
            return -ENOTDIR;
        }

        if(FindDir(node, ent->name)){
            Log::Info("[Ext2] CreateDirectory: Entry %s already exists!", ent->name);
            return -EEXIST;
        }

        Ext2Node* dir = CreateNode();
        
        if(!dir){
            Log::Error("[Ext2] Could not create inode!");
            return -1;
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

        if(int e = InsertDir(dir, entries)){
            return e;
        }

        if(int e = InsertDir(node, *ent)){
            return e;
        }
        
        dir->nlink = dir->e2inode.linkCount;
        node->nlink = node->e2inode.linkCount;
        SyncNode(dir);
        SyncNode(node);

        return 0;
    }
    
    ssize_t Ext2Volume::ReadLink(Ext2Node* node, char* pathBuffer, size_t bufSize){
        if((node->flags & S_IFMT) != S_IFLNK){
            return -EINVAL; // Not a symbolic link
        }

        if(node->size <= 60){
            size_t copySize = MIN(bufSize, node->size);
            strncpy(pathBuffer, (const char*)node->e2inode.blocks, copySize); // Symlinks up to 60 bytes are stored in the blocklist

            return copySize;
        } else {
            return this->Read(node, 0, bufSize, (uint8_t*)pathBuffer);
        }
    }

    int Ext2Volume::Link(Ext2Node* node, Ext2Node* file, DirectoryEntry* ent){
        ent->inode = file->inode;
        if(!ent->inode){
            Log::Error("[Ext2] Link: Invalid inode %d", ent->inode);
            return -EINVAL;
        }

        List<DirectoryEntry> entries;
        if(int e = ListDir(node, entries)){
            Log::Error("[Ext2] Link: Error listing directory!", ent->inode);
            return e;
        }

        for(DirectoryEntry& dirent : entries){
            if(strcmp(dirent.name, ent->name) == 0){
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

    int Ext2Volume::Unlink(Ext2Node* node, DirectoryEntry* ent, bool unlinkDirectories){
        List<DirectoryEntry> entries;
        if(int e = ListDir(node, entries)){
            Log::Error("[Ext2] Unlink: Error listing directory!", ent->inode);
            return e;
        }

        for(unsigned i = 0; i < entries.get_length(); i++) {
            if(strcmp(entries[i].name, ent->name) == 0){
                ent->inode = entries.remove_at(i).inode;
                goto found;
            }
        }
        
        Log::Error("[Ext2] Unlink: Directory entry %s does not exist!", ent->name);
        return -ENOENT;

    found:
        if(!ent->inode){
            Log::Error("[Ext2] Unlink: Invalid inode %d", ent->inode);
            return -EINVAL;
        }

        if(Ext2Node* file = inodeCache.get(ent->inode)){
            if((file->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
                if(!unlinkDirectories){
                    return -EISDIR;
                }
            }

            file->nlink--;
            file->e2inode.linkCount--;

            if(!file->handleCount){
                inodeCache.remove(file->inode);
                CleanNode(file);
            }
        } else {
            ext2_inode_t e2inode;
            if(ReadInode(ent->inode, e2inode)){
                Log::Error("[Ext2] Link: Error reading inode %d", ent->inode);
                return -1;
            }

            if((e2inode.mode & EXT2_S_IFMT) == EXT2_S_IFDIR){
                if(!unlinkDirectories){
                    return -EISDIR;
                }
            }
            
            e2inode.linkCount--;

            if(e2inode.linkCount){
                SyncInode(e2inode, ent->inode);
            } else { // Last link, delete inode
                EraseInode(e2inode, ent->inode);
            }
        }

        return WriteDir(node, entries);
    }

    int Ext2Volume::Truncate(Ext2Node* node, off_t length){
        if(length < 0){
            return -EINVAL;
        }

        if(length > node->e2inode.blockCount * 512){ // We need to allocate blocks
            uint64_t blocksNeeded = (length + blocksize - 1) / blocksize;
            uint64_t blocksAllocated = node->e2inode.blockCount / (blocksize / 512);

            while(blocksAllocated < blocksNeeded){
                SetInodeBlock(blocksAllocated++, node->e2inode, AllocateBlock());
            }

            node->e2inode.blockCount = blocksNeeded * (blocksize / 512);
        }

        node->size = node->e2inode.size = length; // TODO: Actually free blocks in inode if possible

        SyncNode(node);

        return 0;
    }

    void Ext2Volume::CleanNode(Ext2Node* node){
        if(node->handleCount > 0){
            Log::Warning("[Ext2] CleanNode: Node (inode %d) is referenced by %d handles", node->inode, node->handleCount);
            return;
        }

        if(node->e2inode.linkCount == 0){ // No links to file
            EraseInode(node->e2inode, node->inode);
        }

        inodeCache.remove(node->inode);
        delete node;
    }

    Ext2Node::Ext2Node(Ext2Volume* vol, ext2_inode_t& ino, ino_t inode){
        this->vol = vol;
        volumeID = vol->volumeID;

        uid = ino.uid;
        size = ino.size;
        nlink = ino.linkCount;
        this->inode = inode;

        switch (ino.mode & EXT2_S_IFMT)
        {
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

    int Ext2Node::ReadDir(DirectoryEntry* ent, uint32_t idx){
        flock.AcquireRead();
        auto ret = vol->ReadDir(this, ent, idx);
        flock.ReleaseRead();
        return ret;
    }

    FsNode* Ext2Node::FindDir(char* name){
        flock.AcquireRead();
        auto ret = vol->FindDir(this, name);
        flock.ReleaseRead();
        return ret;
    }

    ssize_t Ext2Node::Read(size_t offset, size_t size, uint8_t* buffer){
        flock.AcquireRead();
        auto ret = vol->Read(this, offset, size, buffer);
        flock.ReleaseRead();
        return ret;
    }

    ssize_t Ext2Node::Write(size_t offset, size_t size, uint8_t* buffer){
        flock.AcquireWrite();
        auto ret = vol->Write(this, offset, size, buffer);
        flock.ReleaseWrite();
        return ret;
    }

    int Ext2Node::Create(DirectoryEntry* ent, uint32_t mode){
        flock.AcquireWrite();
        auto ret = vol->Create(this, ent, mode);
        flock.ReleaseWrite();
        return ret;
    }

    int Ext2Node::CreateDirectory(DirectoryEntry* ent, uint32_t mode){
        flock.AcquireWrite();
        auto ret = vol->CreateDirectory(this, ent, mode);
        flock.ReleaseWrite();
        return ret;
    }

    ssize_t Ext2Node::ReadLink(char* pathBuffer, size_t bufSize){
        flock.AcquireRead();
        auto ret = vol->ReadLink(this, pathBuffer, bufSize);
        flock.ReleaseRead();
        return ret;
    }
    
    int Ext2Node::Link(FsNode* n, DirectoryEntry* d){
        if(!n->inode){
            Log::Warning("[Ext2] Ext2Node::Link: Invalid inode %d", n->inode);
            return -EINVAL;
        }

        flock.AcquireWrite();
        auto ret = vol->Link(this, (Ext2Node*)n, d);
        flock.ReleaseWrite();
        return ret;
    }
    
    int Ext2Node::Unlink(DirectoryEntry* d, bool unlinkDirectories){
        flock.AcquireWrite();
        auto ret = vol->Unlink(this, d, unlinkDirectories);
        flock.ReleaseWrite();
        return ret;
    }

    int Ext2Node::Truncate(off_t length){
        flock.AcquireWrite();
        auto ret = vol->Truncate(this, length);
        flock.ReleaseWrite();
        return ret;
    }

    void Ext2Node::Sync(){
        vol->SyncNode(this);
    }

    void Ext2Node::Close(){
        handleCount--;

        if(handleCount == 0){
            vol->CleanNode(this); // Remove from the cache
            return;
        }
    }
}