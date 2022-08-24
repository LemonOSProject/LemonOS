#pragma once

#include <abi/stat.h>

enum class FileType {
    Invalid = 0,
    Regular = S_IFREG,
    BlockDevice = S_IFBLK,
    CharDevice = S_IFCHR,
    Pipe = S_IFIFO,
    Directory = S_IFDIR,
    SymbolicLink = S_IFLNK,
    Socket = S_IFSOCK,
};
