#pragma once

#include <Fs/Filesystem.h>
#include <Fs/FsVolume.h>

#include <Hash.h>
#include <Spinlock.h>

#define TEMP_BUFFER_CHUNK_SIZE 1024UL // Must be power of two
static_assert((TEMP_BUFFER_CHUNK_SIZE & (TEMP_BUFFER_CHUNK_SIZE - 1)) == 0);

namespace fs::Temp{
    class TempVolume;

    class TempNode final : public FsNode {
        friend class TempVolume;
    protected:
        TempVolume* vol;
        TempNode* parent;

        union {
            struct{
                List<DirectoryEntry> children;
            };
            struct {
                ReadWriteLock bufferLock;
                uint8_t* buffer;
                size_t bufferSize;
            };
        };

        TempNode* Find(const char* name);
    public:
        TempNode(TempVolume* v, FileType type);
        ~TempNode();

        ErrorOr<ssize_t> read(size_t off, size_t size, UIOBuffer* buffer); // Read Data
        ErrorOr<ssize_t> write(size_t off, size_t size, UIOBuffer* buffer); // Write Data

        Error truncate(off_t length); // Truncate file

        ErrorOr<int> read_dir(DirectoryEntry*, uint32_t); // Read Directory
        ErrorOr<FsNode*> find_dir(const char* name); // Find in directory

        Error create(DirectoryEntry* entry, uint32_t mode); // Create regular file
        Error create_directory(DirectoryEntry* entry, uint32_t mode); // Create directory
        
        Error link(FsNode*, DirectoryEntry*); // Link
        Error unlink(DirectoryEntry*, bool unlinkDirectories = false); // Unlink

        ErrorOr<File*> Open(size_t flags);
        void Close();
    };

    class TempVolume : public FsVolume {
        friend class TempNode;
    protected:
        uint32_t nextInode = 1;
        HashMap<uint32_t, TempNode*> nodes;
        TempNode* tempMountPoint;
        unsigned memoryUsage = 0;

        void ReallocateNode(TempNode* node, size_t old);
    public:

        TempVolume(const char* name);

        void SetVolumeID(volume_id_t id);

        inline unsigned GetMemoryUsage() { return memoryUsage; }
    };
}