#include <Fs/Fat32.h>

#include <CString.h>
#include <Device.h>
#include <Errno.h>
#include <Logging.h>
#include <Memory.h>

namespace fs::FAT32 {

int Identify(PartitionDevice* part) {
    fat32_boot_record_t* bootRecord = (fat32_boot_record_t*)kmalloc(512);

    if (part->ReadBlock(0, 512, (uint8_t*)bootRecord)) { // Read Volume Boot Record (First sector of partition)
        return -1;                                       // Disk Error
    }

    int isFat = 0;

    if (bootRecord->ebr.signature == 0x28 || bootRecord->ebr.signature == 0x29) {
        uint32_t dataSectors =
            bootRecord->bpb.largeSectorCount -
            (bootRecord->bpb.reservedSectors + (bootRecord->ebr.sectorsPerFAT * bootRecord->bpb.fatCount));
        uint32_t clusters = dataSectors / bootRecord->bpb.sectorsPerCluster;

        if (clusters > 65525)
            isFat = 1;
    }

    kfree(bootRecord);

    return isFat;
}

uint64_t Fat32Volume::ClusterToLBA(uint32_t cluster) {
    return (bootRecord->bpb.reservedSectors + bootRecord->bpb.fatCount * bootRecord->ebr.sectorsPerFAT) +
           cluster * bootRecord->bpb.sectorsPerCluster - (2 * bootRecord->bpb.sectorsPerCluster);
}

Fat32Volume::Fat32Volume(PartitionDevice* _part, char* name) {
    this->part = _part;

    fat32_boot_record_t* bootRecord = (fat32_boot_record_t*)kmalloc(512);

    if (part->ReadBlock(0, 512, (uint8_t*)bootRecord)) { // Read Volume Boot Record (First sector of partition)
        Log::Warning("Disk Error Initializing Volume");  // Disk Error
        return;
    }

    this->bootRecord = bootRecord;

    Log::Info("[FAT32] Initializing Volume\tSignature: %d, OEM ID: %s, Size: %d MB", bootRecord->ebr.signature,
              (char*)bootRecord->bpb.oem, bootRecord->bpb.largeSectorCount * 512 / 1024 / 1024);

    clusterSizeBytes = bootRecord->bpb.sectorsPerCluster * part->parentDisk->blocksize;

    fat32MountPoint.flags = FS_NODE_MOUNTPOINT | FS_NODE_DIRECTORY;
    fat32MountPoint.inode = bootRecord->ebr.rootClusterNum;

    fat32MountPoint.vol = this;
    fat32MountPoint.size = 0;

    mountPoint = &fat32MountPoint;

    mountPointDirent.flags = DT_DIR;
    mountPointDirent.node = &fat32MountPoint;
    strcpy(mountPointDirent.name, name);
}

List<uint32_t>* Fat32Volume::GetClusterChain(uint32_t cluster) {
    List<uint32_t>* list = new List<uint32_t>();

    uint32_t* buf = (uint32_t*)kmalloc(4096);

    uint32_t lastBlock = 0xFFFFFFFF;

    do {
        list->add_back(cluster);

        uint32_t block = ((cluster * 4) / 4096);
        uint32_t offset = cluster % (4096 / 4);

        if (block != lastBlock) {
            if (part->ReadBlock(bootRecord->bpb.reservedSectors +
                                    block * (4096 / part->parentDisk->blocksize) /* Get Sector of Block */,
                                4096, buf)) {
                delete list;
                return nullptr;
            }

            lastBlock = block;
        }

        cluster = buf[offset] & 0x0FFFFFFF;

    } while (cluster && (cluster & 0x0FFFFFFF) < 0x0FFFFFF8);

    return list;
}

void* Fat32Volume::ReadClusterChain(uint32_t cluster, int* clusterCount, size_t max) {
    uint32_t maxCluster = (max / (bootRecord->bpb.sectorsPerCluster * part->parentDisk->blocksize)) + 1;
    List<uint32_t>* clusterChain = GetClusterChain(cluster);

    if (!clusterChain)
        return nullptr;

    uint8_t* buf = reinterpret_cast<uint8_t*>(kmalloc(clusterChain->get_length() * clusterSizeBytes));
    void* _buf = buf;

    for (unsigned i = 0; i < clusterChain->get_length() && maxCluster; i++) {
        part->ReadBlock(ClusterToLBA(clusterChain->get_at(i)), clusterSizeBytes, buf);

        buf += clusterSizeBytes;
    }

    if (clusterCount)
        *clusterCount = clusterChain->get_length();

    delete clusterChain;

    return _buf;
}

void* Fat32Volume::ReadClusterChain(uint32_t cluster, int* clusterCount) {
    if (cluster == 0)
        cluster = bootRecord->ebr.rootClusterNum;
    List<uint32_t>* clusterChain = GetClusterChain(cluster);

    if (!clusterChain)
        return nullptr;

    uint8_t* buf = reinterpret_cast<uint8_t*>(kmalloc(clusterChain->get_length() * clusterSizeBytes));
    void* _buf = buf;

    for (unsigned i = 0; i < clusterChain->get_length(); i++) {
        part->ReadBlock(ClusterToLBA(clusterChain->get_at(i)), clusterSizeBytes, buf);

        buf += clusterSizeBytes;
    }

    if (clusterCount)
        *clusterCount = clusterChain->get_length();

    delete clusterChain;

    return _buf;
}

ssize_t Fat32Volume::Read(Fat32Node* node, size_t offset, size_t size, uint8_t* buffer) {
    if (!node->inode || node->flags & FS_NODE_DIRECTORY)
        return -1;

    int count;
    void* _buf = ReadClusterChain(node->inode, &count, size);

    if (static_cast<unsigned>(count) * bootRecord->bpb.sectorsPerCluster * part->parentDisk->blocksize < size)
        return 0;

    memcpy(buffer, _buf, size);
    return size;
}

ssize_t Fat32Volume::Write(Fat32Node* node, size_t offset, size_t size, uint8_t* buffer) {
    return -EROFS;

    List<uint32_t>* clusters = GetClusterChain(node->inode);
    if (offset + size > clusters->get_length() * bootRecord->bpb.sectorsPerCluster * part->parentDisk->blocksize) {
        // TODO: Allocate clusters
        Log::Info("[FAT32] Allocating Clusters");
    }
}

void Fat32Volume::Open(Fat32Node* node, uint32_t flags) {}

void Fat32Volume::Close(Fat32Node* node) {}

int Fat32Volume::ReadDir(Fat32Node* node, DirectoryEntry* dirent, uint32_t index) {
    unsigned lfnCount = 0;
    unsigned entryCount = 0;

    uint32_t cluster = node->inode;
    int clusterCount = 0;

    fat_entry_t* dirEntries = (fat_entry_t*)ReadClusterChain(cluster, &clusterCount);

    fat_entry_t* dirEntry;
    int dirEntryIndex = -1;

    fat_lfn_entry_t** lfnEntries;

    for (unsigned i = 0; i < static_cast<unsigned>(clusterCount) * clusterSizeBytes / sizeof(fat_entry_t); i++) {
        if (dirEntries[i].filename[0] == 0)
            continue; // No Directory Entry at index
        else if (dirEntries[i].filename[0] == 0xE5) {
            lfnCount = 0;
            continue; // Unused Entry
        } else if (dirEntries[i].attributes == 0x0F)
            lfnCount++; // Long File Name Entry
        else if (dirEntries[i].attributes & 0x08 /*Volume ID*/) {
            lfnCount = 0;
            continue;
        } else {
            if (entryCount == index) {
                dirEntry = &dirEntries[i];
                dirEntryIndex = i;
                break;
            }

            entryCount++;
            lfnCount = 0;
        }
    }

    if (dirEntryIndex == -1) {
        return 0;
    }

    lfnEntries = (fat_lfn_entry_t**)kmalloc(sizeof(fat_lfn_entry_t*) * lfnCount);

    for (unsigned i = 0; i < lfnCount; i++) {
        fat_lfn_entry_t* lfnEntry = (fat_lfn_entry_t*)(&dirEntries[dirEntryIndex - i - 1]);

        lfnEntries[i] = lfnEntry;
    }

    if (lfnCount) {
        GetLongFilename(dirent->name, lfnEntries, lfnCount);
    } else {
        strncpy(dirent->name, (char*)dirEntry->filename, 8);
        while (strchr(dirent->name, ' '))
            *strchr(dirent->name, ' ') = 0; // Remove Spaces
        if (strchr((char*)dirEntry->ext, ' ') != (char*)dirEntry->ext) {
            strncpy(dirent->name + strlen(dirent->name), ".", 1);
            strncpy(dirent->name + strlen(dirent->name), (char*)dirEntry->ext, 3);
        }
    }

    if (dirEntry->attributes & FAT_ATTR_DIRECTORY)
        dirent->flags = DT_DIR;
    else
        dirent->flags = DT_REG;

    return 1;
}

FsNode* Fat32Volume::FindDir(Fat32Node* node, const char* name) {
    unsigned lfnCount = 0;

    uint32_t cluster = node->inode;
    int clusterCount = 0;

    fat_entry_t* dirEntries = (fat_entry_t*)ReadClusterChain(cluster, &clusterCount);

    List<int> foundEntries;
    List<int> foundEntriesLfnCount;

    fat_lfn_entry_t** lfnEntries;
    Fat32Node* _node = nullptr;

    for (unsigned i = 0; i < static_cast<unsigned>(clusterCount) * clusterSizeBytes; i++) {
        if (dirEntries[i].filename[0] == 0)
            return nullptr; // No Directory Entry at index
        else if (dirEntries[i].filename[0] == 0xE5) {
            lfnCount = 0;
            continue; // Unused Entry
        } else if (dirEntries[i].attributes == 0x0F)
            lfnCount++; // Long File Name Entry
        else if (dirEntries[i].attributes & 0x08 /*Volume ID*/) {
            lfnCount = 0;
            continue;
        } else {
            char* _name = (char*)kmalloc(128);
            if (lfnCount) {
                lfnEntries = (fat_lfn_entry_t**)kmalloc(sizeof(fat_lfn_entry_t*) * lfnCount);

                for (unsigned k = 0; k < lfnCount; k++) {
                    fat_lfn_entry_t* lfnEntry = (fat_lfn_entry_t*)(&dirEntries[i - k - 1]);

                    lfnEntries[k] = lfnEntry;
                }

                GetLongFilename(_name, lfnEntries, lfnCount);

                kfree(lfnEntries);
            } else {
                strncpy(_name, (char*)dirEntries[i].filename, 8);
                while (strchr(_name, ' '))
                    *strchr(_name, ' ') = 0; // Remove Spaces
                if (strchr((char*)dirEntries[i].ext, ' ') != (char*)dirEntries[i].ext) {
                    strncpy(_name + strlen(_name), ".", 1);
                    strncpy(_name + strlen(_name), (char*)dirEntries[i].ext, 3);
                }
            }

            if (strcmp(_name, name) == 0) {
                uint64_t clusterNum = (((uint32_t)dirEntries[i].highClusterNum) << 16) | dirEntries[i].lowClusterNum;
                if (clusterNum == bootRecord->ebr.rootClusterNum || clusterNum == 0)
                    return mountPoint; // Root Directory
                _node = new Fat32Node();
                _node->size = dirEntries[i].fileSize;
                _node->inode = clusterNum;
                if (dirEntries[i].attributes & FAT_ATTR_DIRECTORY)
                    _node->flags = FS_NODE_DIRECTORY;
                else
                    _node->flags = FS_NODE_FILE;
                break;
            }
            lfnCount = 0;
        }
    }

    if (_node) {
        _node->vol = this;
    }

    return _node;
}

ssize_t Fat32Node::Read(size_t offset, size_t size, uint8_t* buffer) { return vol->Read(this, offset, size, buffer); }

ssize_t Fat32Node::Write(size_t offset, size_t size, uint8_t* buffer) { return vol->Write(this, offset, size, buffer); }

int Fat32Node::ReadDir(DirectoryEntry* dirent, uint32_t index) { return vol->ReadDir(this, dirent, index); }

FsNode* Fat32Node::FindDir(const char* name) { return vol->FindDir(this, name); }
} // namespace fs::FAT32