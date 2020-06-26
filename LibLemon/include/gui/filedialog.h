#pragma once

#define FILE_DIALOG_CREATE 0x1
#define FILE_DIALOG_DIRECTORIES 0x2

namespace Lemon::GUI{
    char* FileDialog(const char* path, int flags = 0);
}