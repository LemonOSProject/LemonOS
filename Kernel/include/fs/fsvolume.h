#pragma once

#include <fs/filesystem.h>

namespace fs{

    class FsVolume{
    public:

        FsNode* mountPoint;
        fs_dirent_t mountPointDirent;
    };    

    class LinkVolume : public FsVolume{
    public:
        FsNode linkMountPoint;

        LinkVolume(FsVolume* link, char* name){
            linkMountPoint = *link->mountPoint;
            mountPoint = &linkMountPoint;
            linkMountPoint.flags = FS_NODE_SYMLINK | FS_NODE_DIRECTORY | FS_NODE_MOUNTPOINT;
            linkMountPoint.link = link->mountPoint;
            strcpy(linkMountPoint.name, name);
            strcpy(mountPointDirent.name, linkMountPoint.name);
            mountPointDirent.type = FS_NODE_SYMLINK;
        }
    };
}