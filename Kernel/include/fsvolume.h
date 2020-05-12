#pragma once

#include <filesystem.h>

namespace fs{

    class FsVolume{
    public:
        /*virtual size_t Read(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer) = 0;
        virtual size_t Write(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer) = 0;
        virtual void Open(fs_node_t* node, uint32_t flags) = 0;
        virtual void Close(fs_node_t* node) = 0;
        static virtual fs_dirent_t* ReadDir(fs_node_t* node, uint32_t index) = 0;
        virtual fs_node_t* FindDir(fs_node_t* node, char* name) = 0;*/

        fs_node_t mountPoint;
        fs_dirent_t mountPointDirent;

        fs_node_t* volumeParent;
    };    

    class LinkVolume : public FsVolume{
    public:
        LinkVolume(FsVolume* link, char* name){
            mountPoint = link->mountPoint;
            mountPoint.flags = FS_NODE_SYMLINK | FS_NODE_DIRECTORY | FS_NODE_MOUNTPOINT;
            mountPoint.link = &link->mountPoint;
            strcpy(mountPoint.name, name);
            strcpy(mountPointDirent.name, mountPoint.name);
            mountPointDirent.type = FS_NODE_SYMLINK;
            volumeParent = link->volumeParent;
        }
    };
}