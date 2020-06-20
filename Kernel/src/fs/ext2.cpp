#include <fs/ext2.h>

#include <logging.h>
#include <errno.h>
#include <assert.h>

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
            error = true;
            return; // Disk Error
        }

        if(super.revLevel){ // If revision level >= 0 grab the extended superblock as well
            if(part->Read(EXT2_SUPERBLOCK_LOCATION / part->parentDisk->blocksize, sizeof(ext2_superblock_t) + sizeof(ext2_superblock_extended_t), &super)){
                Log::Error("[Ext2] Disk Error Initializing Volume");
                error = true;
                return; // Disk Error
            }

            if((superext.featuresIncompat & (~EXT2_INCOMPAT_FEATURE_SUPPORT)) != 0 || (superext.featuresRoCompat & (~EXT2_READONLY_FEATURE_SUPPORT)) != 0){ // Check support for incompatible/read-only compatible features
                Log::Error("[Ext2] Incompatible Ext2 features present (Incompt: %x, Read-only Compt: %x). Will not mount volume.", superext.featuresIncompat, superext.featuresRoCompat);
                error = true;
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
            error = true;
            return;
        }

        blocksize = 1024U << super.logBlockSize;
        superBlockIndex = LocationToBlock(EXT2_SUPERBLOCK_LOCATION);

        Log::Info("[Ext2] Initializing Volume\tRevision: %d, Block Size: %d, %d KB/%d KB used, Last mounted on: %s", super.revLevel, blocksize, super.freeBlockCount * blocksize / 1024, super.blockCount * blocksize / 1024, superext.lastMounted);
        Log::Info("[Ext2] Block Group Count: %d, Inodes Per Block Group: %d, Inode Size: %d", blockGroupCount, super.inodesPerGroup, inodeSize);

        blockGroups = (ext2_blockgrp_desc_t*)kmalloc(blockGroupCount * sizeof(ext2_blockgrp_desc_t));
        
        uint64_t blockGLBA = BlockToLBA(LocationToBlock(EXT2_SUPERBLOCK_LOCATION) + 1); // One block from the superblock
        
        if(part->Read(blockGLBA, blockGroupCount * sizeof(ext2_blockgrp_desc_t), blockGroups)){
            Log::Error("[Ext2] Disk Error Initializing Volume");
            error = true;
            return; // Disk Error
        }

        if(super.revLevel){
            inodeSize = superext.inodeSize;
        } else {
            inodeSize = 128;
        }

        mountPoint = new Ext2Node(this);
        mountPoint->flags = FS_NODE_DIRECTORY;
        mountPoint->inode = EXT2_ROOT_INODE_INDEX;
        mountPointDirent.node = mountPoint; 
        strcpy(mountPointDirent.name, name);

        DirectoryEntry testDirent;
        mountPoint->ReadDir(&testDirent, 4);
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
            uint32_t* buffer = (uint32_t*)kmalloc(blocksize);

            if(int e = ReadBlock(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)){
                kfree(buffer);
                error = 1;
                return 0;
            }

            kfree(buffer);

            return buffer[index - singlyIndirectStart];
        } else if(index < triplyIndirectStart){
            // Index lies within the doubly indirect blocklist
            uint32_t* blockPointers = (uint32_t*)kmalloc(blocksize);
            uint32_t* buffer = (uint32_t*)kmalloc(blocksize);

            if(int e = ReadBlock(ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX], blockPointers)){
                kfree(blockPointers);
                kfree(buffer);
                error = 1;
                return 0;
            }

            uint32_t blockPointer = blockPointers[(index - doublyIndirectStart) / blocksPerPointer];

            kfree(blockPointers);

            if(int e = ReadBlock(blockPointer, buffer)){
                kfree(buffer);
                error = 1;
                return 0;
            }

            kfree(buffer);

            return buffer[(index - doublyIndirectStart) % blocksPerPointer];
        } else {
            assert(!"Yet to support triply indirect");
            return 0;
        }
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
            uint32_t* buffer = (uint32_t*)kmalloc(blocksize);

            if(int e = ReadBlock(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)){
                kfree(buffer);
                error = 1;
                return;
            }

            buffer[index - singlyIndirectStart] = block;
            
            if(int e = WriteBlock(ino.blocks[EXT2_SINGLY_INDIRECT_INDEX], buffer)){
                kfree(buffer);
                error = 2;
                return;
            }

            kfree(buffer);
            return;
        } else if(index < triplyIndirectStart){
            // Index lies within the doubly indirect blocklist
            uint32_t* blockPointers = (uint32_t*)kmalloc(blocksize);
            uint32_t* buffer = (uint32_t*)kmalloc(blocksize);

            if(int e = ReadBlock(ino.blocks[EXT2_DOUBLY_INDIRECT_INDEX], blockPointers)){ // Read indirect block pointer list
                kfree(blockPointers);
                kfree(buffer);
                error = 1;
                return;
            }

            uint32_t blockPointer = blockPointers[(index - doublyIndirectStart) / blocksPerPointer];

            kfree(blockPointers);

            if(int e = ReadBlock(blockPointer, buffer)){ // Read blocklist
                kfree(buffer);
                error = 1;
                return;
            }

            buffer[(index - doublyIndirectStart) % blocksPerPointer] = block; // Update the index
            
            if(int e = WriteBlock(blockPointer, buffer)){ // Write our updated blocklist
                kfree(buffer);
                error = 2;
                return;
            }

            kfree(buffer);
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
            Log::Error("[Ext2] Disk Error Reading Inode %d", num);
            error = true;
            return e;
        }

        inode = *(ext2_inode_t*)(buf + (ResolveInodeBlockGroupIndex(num) * inodeSize) % part->parentDisk->blocksize);
        return 0;
    }

    int Ext2Volume::ReadBlock(uint32_t block, void* buffer){
        if(block > super.blockCount)
            return -1;

        if(int e = part->Read(BlockToLBA(block), blocksize, buffer)){
            Log::Error("[Ext2] Disk error reading block %d (blocksize: %d)", block, blocksize);
            return e;
        }

        return 0;
    }
    
    int Ext2Volume::WriteBlock(uint32_t block, void* buffer){
        if(block > super.blockCount)
            return -1;

        if(int e = part->Write(BlockToLBA(block), blocksize, buffer)){
            Log::Error("[Ext2] Disk error reading block %d (blocksize: %d)", block, blocksize);
            return e;
        }

        return 0;
    }
    
    uint32_t Ext2Volume::AllocateBlock(){
        for(unsigned i = 0; i < blockGroupCount; i++){
            ext2_blockgrp_desc_t& group = blockGroups[i];

            if(group.freeBlockCount <= 0) continue; // No free blocks in this blockgroup

            uint8_t bitmap[blocksize / sizeof(uint8_t)];

            if(int e = ReadBlock(group.blockBitmap, bitmap)){
                Log::Error("[Ext2] Disk error reading block bitmap (group %d)", i);
                error = e;
                return 0;
            }

            uint32_t block = 0;

            for(unsigned e = 0; e < blocksize / sizeof(uint8_t); e++){
                if(bitmap[e] == UINT8_MAX) continue; // Full

                for(uint8_t b = 0; b < sizeof(uint8_t) * 8; b++){ // Iterate through all bits in the entry
                    if(((bitmap[e] >> b) & 0x1) == 0){
                        bitmap[e] |= (1U << b);
                        block = (i * super.blocksPerGroup) + e * (sizeof(uint8_t) * 8) + b; // Block Number = (Group number * blocks per group) + (bitmap entry * 8 (8 bits per byte)) + bit (block 0 is least significant bit, block 7 is most significant)
                        break;
                    }
                }
            }

            assert(block); // Make sure a block was given

            if(int e = WriteBlock(group.blockBitmap, bitmap)){
                Log::Error("[Ext2] Disk error write block bitmap (group %d)", i);
                error = e;
                return 0;
            }

            return block;
        }

        Log::Error("[Ext2] No space left on filesystem!");
        return 0;
    }

    int Ext2Volume::ReadDir(Ext2Node* node, DirectoryEntry* dirent, uint32_t index){
        if(!(node->flags & FS_NODE_DIRECTORY)){
            return -ENOTDIR;
        }

        if(node->inode < 1){
            Log::Warning("[Ext2] ReadDir: Invalid inode: %d", node->inode);
            return -1;
        }

        ext2_inode_t ino;
        if(ReadInode(node->inode, ino)){
            Log::Error("[Ext2] ReadDir: Failed to read inode");
        }

        uint8_t* buffer = (uint8_t*)kmalloc(blocksize);
        uint32_t currentBlockIndex = 0;
        uint32_t blockOffset = 0;
        uint32_t totalOffset = 0;

        ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)buffer;

        Log::Info("Reading block %d", ino.blocks[0]);
        if(ReadBlock(GetInodeBlock(currentBlockIndex, ino), buffer)){
            return -1;
        }

        for(int i = 0; i < index; i++){
            Log::Info("Found directory entry: %s", e2dirent->name);
        
            blockOffset += e2dirent->recordLength;
            totalOffset += e2dirent->recordLength;

            if(totalOffset > ino.size || e2dirent->recordLength == 0) return -2;

            if(blockOffset > blocksize){
                currentBlockIndex++;
                blockOffset = 0;
                if(ReadBlock(GetInodeBlock(currentBlockIndex, ino), buffer)){
                    return -1;
                }
            }

            e2dirent = (ext2_directory_entry_t*)(buffer + blockOffset);
        }

        Log::Info("Found directory entry: %s", e2dirent->name);

        strcpy(dirent->name, e2dirent->name);
        dirent->flags = FS_NODE_FILE;

        return 0;
    }

    FsNode* Ext2Volume::FindDir(Ext2Node* node, char* name){
        if(!(node->flags & FS_NODE_DIRECTORY)){
            return nullptr;
        }

        if(node->inode < 1){
            Log::Warning("[Ext2] ReadDir: Invalid inode: %d", node->inode);
            return nullptr;
        }

        ext2_inode_t ino;
        if(ReadInode(node->inode, ino)){
            Log::Error("[Ext2] ReadDir: Failed to read inode");
        }

        uint8_t* buffer = (uint8_t*)kmalloc(blocksize);
        uint32_t currentBlockIndex = 0;
        uint32_t blockOffset = 0;
        uint32_t totalOffset = 0;

        ext2_directory_entry_t* e2dirent = (ext2_directory_entry_t*)buffer;

        Log::Info("Reading block %d", ino.blocks[0]);
        if(ReadBlock(GetInodeBlock(currentBlockIndex, ino), buffer)){
            return nullptr;
        }

        while(totalOffset < ino.size){
            if(strcmp(e2dirent->name, name) == 0){
                break;
            }
        
            blockOffset += e2dirent->recordLength;
            totalOffset += e2dirent->recordLength;

            if(totalOffset > ino.size) return nullptr;

            if(blockOffset > blocksize){
                currentBlockIndex++;
                blockOffset = 0;
                if(ReadBlock(GetInodeBlock(currentBlockIndex, ino), buffer)){
                    return nullptr;
                }
            }

            e2dirent = (ext2_directory_entry_t*)(buffer + blockOffset);
        }

        if(strcmp(e2dirent->name, name) != 0){
            // Not found
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

        return returnNode;
    }

    size_t Ext2Volume::Read(Ext2Node* node, size_t offset, size_t size, uint8_t *buffer){
        if(offset >= node->size) return 0;
        if(offset + size > node->size) size = node->size - offset;

        uint32_t blockIndex = LocationToBlock(offset);
        uint32_t blockCount = LocationToBlock(size) - blockIndex + 1;
        uint8_t* blockBuffer = (uint8_t*)kmalloc(blocksize);

        Log::Info("[Ext2] Reading: Block index: %d, Blockcount: %d, Offset: %d, Size: %d", blockIndex, blockCount, offset, size);

        size_t ret = size;

        for(unsigned i = 0; i < blockCount && size > 0; i++, blockIndex++){
            uint32_t block = GetInodeBlock(blockIndex, node->e2inode);
            ReadBlock(block, blockBuffer);
            
            if(offset % blocksize){
                size_t readSize = blocksize - (offset % blocksize);
                size_t readOffset = (offset % blocksize);
                memcpy(buffer, blockBuffer + readOffset, readSize);
                size -= readSize;
                buffer += readSize;
                offset += readSize;
            } else if(size >= blocksize){
                memcpy(buffer, blockBuffer, blocksize);
                size -= blocksize;
                buffer += blocksize;
                offset += blocksize;
            } else {
                memcpy(buffer, blockBuffer, size);
                break;
            }
        }

        return ret;
    }

    size_t Ext2Volume::Write(Ext2Node* node, size_t offset, size_t size, uint8_t *buffer){
        uint32_t blockIndex = LocationToBlock(offset); // Index of first block to write
        uint32_t fileBlockCount = LocationToBlock(node->size) + 1; // Size of file in blocks
        uint32_t blockCount = LocationToBlock(size) + 1; // Amount of blocks to write
        uint8_t* blockBuffer = (uint8_t*)kmalloc(blocksize); // block buffer
        bool sync = false; // Need to sync the inode?

        if(blockCount + blockIndex > fileBlockCount){
            Log::Info("[Ext2] Allocating blocks for inode %d", node->inode);
            for(int i = fileBlockCount; i < blockCount + blockIndex; i++){
                uint32_t block = AllocateBlock();
                SetInodeBlock(i, node->e2inode, block);
            }
            fileBlockCount = blockCount + blockIndex;

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

        Log::Info("[Ext2] Writing: Block index: %d, Blockcount: %d, Offset: %d, Size: %d", blockIndex, blockCount, offset, size);

        size_t ret = size;

        for(unsigned i = 0; i < blockCount && size > 0; i++, blockIndex++){
            uint32_t block = GetInodeBlock(blockIndex, node->e2inode);
            
            if(offset % blocksize){
                ReadBlock(block, blockBuffer);

                size_t writeSize = blocksize - (offset % blocksize);
                size_t writeOffset = (offset % blocksize);

                memcpy(blockBuffer + writeOffset, buffer, writeSize);
                WriteBlock(block, buffer);

                size -= writeSize;
                buffer += writeSize;
                offset += writeSize;
            } else if(size >= blocksize){
                memcpy(blockBuffer, blockBuffer, blocksize);
                WriteBlock(block, buffer);

                size -= blocksize;
                buffer += blocksize;
                offset += blocksize;
            } else {
                ReadBlock(block, blockBuffer);
                memcpy(blockBuffer, buffer, size);
                WriteBlock(block, buffer);
                break;
            }
        }

        return ret;
    }

    void Ext2Volume::SyncNode(Ext2Node* node){
        uint8_t buf[part->parentDisk->blocksize];
        uint64_t lba = InodeLBA(node->inode);

        if(int e = part->Read(lba, part->parentDisk->blocksize, buf)){
            Log::Error("[Ext2] Sync: Disk Error Reading Inode %d", node->inode);
            error = 1;
            return;
        }

        *(ext2_inode_t*)(buf + (ResolveInodeBlockGroupIndex(node->inode) * inodeSize) % part->parentDisk->blocksize) = node->e2inode;

        if(int e = part->Write(lba, part->parentDisk->blocksize, buf)){
            Log::Error("[Ext2] Sync: Disk Error Writing Inode %d", node->inode);
            error = 2;
            return;
        }
    }

    void Ext2Volume::CleanNode(Ext2Node* node){
        inodeCache.remove(node->inode);
        delete node;
    }

    Ext2Node::Ext2Node(Ext2Volume* vol, ext2_inode_t& ino, ino_t inode){
        this->vol = vol;

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
        default:
            flags = FS_NODE_FILE;
            break;
        }

        e2inode = ino;
    }

    int Ext2Node::ReadDir(DirectoryEntry* ent, uint32_t idx){
        return vol->ReadDir(this, ent, idx);
    }

    FsNode* Ext2Node::FindDir(char* name){
        return vol->FindDir(this, name);
    }

    size_t Ext2Node::Read(size_t offset, size_t size, uint8_t* buffer){
        return vol->Read(this, offset, size, buffer);
    }

    size_t Ext2Node::Write(size_t offset, size_t size, uint8_t* buffer){
        return vol->Write(this, offset, size, buffer);
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