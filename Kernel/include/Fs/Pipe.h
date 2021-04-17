#pragma once

#include <Fs/Filesystem.h>
#include <RefPtr.h>
#include <Stream.h>

class UNIXPipe final : public FsNode {
public:
    UNIXPipe(int end, FancyRefPtr<DataStream> stream);

    ssize_t Read(size_t off, size_t size, uint8_t* buffer);
    ssize_t Write(size_t off, size_t size, uint8_t* buffer);

    void Watch(FilesystemWatcher& watcher, int events);
    void Unwatch(FilesystemWatcher& watcher);

    void Close();

    static void CreatePipe(UNIXPipe*& read, UNIXPipe*& write);
protected:
    enum {
        InvalidPipe,
        ReadEnd,
        WriteEnd,
    } end = InvalidPipe;

    bool widowed = false;
    UNIXPipe* otherEnd = nullptr;

    FancyRefPtr<DataStream> stream;
    
    List<FilesystemWatcher*> watching;
    lock_t watchingLock = 0;
};