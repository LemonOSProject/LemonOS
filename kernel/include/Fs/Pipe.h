#pragma once

#include <Objects/File.h>

#include <RefPtr.h>
#include <Stream.h>

#include <Error.h>

class UNIXPipe final : public File {
public:
    UNIXPipe(mode_t mode, FancyRefPtr<DataStream> stream);
    ~UNIXPipe() override;

    ErrorOr<ssize_t> Read(off_t off, size_t size, UIOBuffer* buffer) override;
    ErrorOr<ssize_t> Write(off_t off, size_t size, UIOBuffer* buffer) override;

    void Watch(class FsWatcher* watcher, Fs::FsEvent events) override;
    void Unwatch(class FsWatcher* watcher) override;

    Error Create(FancyRefPtr<UNIXPipe>& read, FancyRefPtr<UNIXPipe>& write);

protected:
    bool widowed = false;
    UNIXPipe* otherEnd = nullptr;

    FancyRefPtr<DataStream> stream;
    
    List<FilesystemWatcher*> watching;
    lock_t watchingLock = 0;
};
