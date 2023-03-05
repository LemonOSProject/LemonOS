#pragma once

#include <Assert.h>
#include <Compiler.h>
#include <Fs/FileType.h>

#include <abi/fcntl.h>
#include <abi/types.h>

class DirectoryEntry {
public:
    char name[NAME_MAX];

    DirectoryEntry* parent = nullptr;

    mode_t flags = 0;
    class FsNode* node = nullptr;
    ino_t ino;
    
    DirectoryEntry(FsNode* node, const char* name);
    DirectoryEntry() {}

    ALWAYS_INLINE FsNode* inode() { return node; }

    static mode_t FileToDirentFlags(FileType type) {
        mode_t flags;
        switch (type) {
        case FileType::Regular:
            flags = DT_REG;
            break;
        case FileType::Directory:
            flags = DT_DIR;
            break;
        case FileType::CharDevice:
            flags = DT_CHR;
            break;
        case FileType::BlockDevice:
            flags = DT_BLK;
            break;
        case FileType::Socket:
            flags = DT_SOCK;
            break;
        case FileType::SymbolicLink:
            flags = DT_LNK;
            break;
        default:
            assert(!"Invalid file flags!");
        }
        return flags;
    }
};
