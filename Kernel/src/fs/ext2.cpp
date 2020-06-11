#include <fs/ext2.h>

#include <logging.h>

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

        Log::Info("[Ext2] Initializing Volume\tRevision: %d, Block Size: %d, %d KB/%d KB used, Last mounted on: %s", super.revLevel, blocksize, super.freeBlockCount * blocksize / 1024, super.blockCount * blocksize / 1024, superext.lastMounted);

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
    }
}