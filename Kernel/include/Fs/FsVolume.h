#pragma once

#include <Fs/Filesystem.h>

#include <String.h>

namespace fs{

    class FsVolume{
    public:
        volume_id_t volumeID;
        FsNode* mountPoint;
        DirectoryEntry mountPointDirent;

        virtual void SetVolumeID(volume_id_t id);

        virtual ~FsVolume() = default;
    };    

    class LinkVolume : public FsVolume{
    public:
        inline LinkVolume(FsVolume* link, char* name){
            strcpy(mountPointDirent.name, name);
            mountPointDirent.node = link->mountPoint;
            mountPointDirent.flags = link->mountPointDirent.flags;
            mountPointDirent.node->nlink++;
        }

        inline LinkVolume(FsNode* link, char* name){
            strcpy(mountPointDirent.name, name);
            mountPointDirent.node = link;
            mountPointDirent.flags = DT_DIR;
            mountPointDirent.node->nlink++;
        }
    };
}