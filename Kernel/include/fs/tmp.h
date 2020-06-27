#pragma once

#include <fs/filesystem.h>
#include <fs/fsvolume.h>
#include <hash.h>

namespace fs::Temp{
    class TempVolume;

    class TempNode : public FsNode {
    protected:
        TempVolume* vol;
        uint8_t* buffer = nullptr;
        List<DirectoryEntry> children;
        size_t bufferSize = 0;

        friend class TempVolume;
    public:
        TempNode(TempVolume* v);

        ssize_t Read(size_t, size_t, uint8_t *); // Read Data
        ssize_t Write(size_t, size_t, uint8_t *); // Write Data

        int ReadDir(DirectoryEntry*, uint32_t); // Read Directory
        FsNode* FindDir(char* name); // Find in directory

        int Create(DirectoryEntry*, uint32_t);
        int CreateDirectory(DirectoryEntry*, uint32_t);
        
        int Link(FsNode*, DirectoryEntry*);
        int Unlink(DirectoryEntry*);
    };

    class TempVolume : public FsVolume {
    protected:
        uint32_t nextInode = 1;
        HashMap<uint32_t, TempNode*> nodes;
        TempNode* tempMountPoint;

        friend class TempNode;
    public:
        unsigned memoryUsage = 0;

        void ReallocateNode(TempNode* node);

        TempVolume(const char* name);
    };
}