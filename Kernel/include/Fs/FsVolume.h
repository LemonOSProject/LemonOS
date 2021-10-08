#pragma once

#include <Fs/Filesystem.h>

#include <String.h>

namespace fs {
class FsVolume {
public:
    volume_id_t volumeID;
    FsNode* mountPoint = nullptr;
    DirectoryEntry mountPointDirent;

    virtual void SetVolumeID(volume_id_t id);

    virtual ~FsVolume() = default;
};

class LinkNode : public FsNode {
public:
    LinkNode(const String& link) : m_link(link) {
        flags = FS_NODE_SYMLINK;
        pmask = 0755;
        uid = 0;
        inode = 0;
        size = link.Length();
    }

    ssize_t ReadLink(char* pathBuffer, size_t bufSize) override { 
        if(bufSize > m_link.Length()){
            memcpy(pathBuffer, m_link.c_str(), m_link.Length()); // Do not null terminate
            return m_link.Length();
        }

        memcpy(pathBuffer, m_link.c_str(), bufSize);
        return bufSize;
    }

private:
    String m_link;
};

class LinkVolume : public FsVolume {
public:
    inline LinkVolume(const String& link, const char* name) {
        strcpy(mountPointDirent.name, name);
        mountPoint = new LinkNode(link);
        mountPointDirent.node = mountPoint;
        mountPointDirent.flags = DT_LNK;
        mountPointDirent.node->nlink++;
    }
};
} // namespace fs