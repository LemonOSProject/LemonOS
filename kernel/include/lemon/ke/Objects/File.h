#pragma once

#include <Error.h>
#include <Spinlock.h>
#include <UserIO.h>

#include <Fs/FileType.h>

#include <Objects/KObject.h>

#include <abi/stat.h>
#include <abi/types.h>

class File : public KernelObject {
    DECLARE_KOBJECT(File);

public:
    virtual ~File() override;

    /////////////////////////////
    /// \brief Read data from file object
    ///
    /// Read data from the file
    ///
    /// \param size Amount of data (in bytes) to read
    /// \param buffer Buffer to write data to
    ///
    /// \return Bytes read or Error
    /////////////////////////////
    ErrorOr<ssize_t> Read(size_t size, UIOBuffer* buffer);                    // Read Data
    virtual ErrorOr<ssize_t> Read(off_t off, size_t size, UIOBuffer* buffer); // Read Data

    /////////////////////////////
    /// \brief Write data to file object
    ///
    /// Write data to the file
    ///
    /// \param size Amount of data (in bytes) to write
    /// \param buffer Buffer to read data from
    ///
    /// \return Bytes written or Error
    /////////////////////////////
    ErrorOr<ssize_t> Write(size_t size, UIOBuffer* buffer);                    // Write Data
    virtual ErrorOr<ssize_t> Write(off_t off, size_t size, UIOBuffer* buffer); // Write Data

    virtual ErrorOr<int> Ioctl(uint64_t cmd, uint64_t arg);

    // Read Directory
    virtual ErrorOr<int> ReadDir(struct DirectoryEntry*, uint32_t);

    virtual ErrorOr<struct MappedRegion*> MMap(uintptr_t base, size_t size, off_t off, int prot, bool shared,
                                              bool fixed);

    virtual void Watch(class KernelObjectWatcher* watcher, KOEvent events) override;
    virtual void Unwatch(class KernelObjectWatcher* watcher) override;

    ALWAYS_INLINE bool IsFile() const { return type == FileType::Regular; }
    ALWAYS_INLINE bool IsBlockDev() const { return type == FileType::BlockDevice; }
    ALWAYS_INLINE bool IsCharDev() const { return type == FileType::CharDevice; }
    ALWAYS_INLINE bool IsPipe() const { return type == FileType::Pipe; }
    ALWAYS_INLINE bool IsDirectory() const { return type == FileType::Directory; }
    ALWAYS_INLINE bool IsSymbolicLink() const { return type == FileType::SymbolicLink; }
    ALWAYS_INLINE bool IsSocket() const { return type == FileType::Socket; }

    lock_t fLock = 0;

    off_t pos = 0;
    mode_t mode = 0;

    FileType type;

    // For inode-based filesystems
    // ANY regular file or directory should have an inode attached
    class FsNode* inode = nullptr;

    // e.g. O_NONBLOCK
    unsigned int flags;
};

// inode backed file
// (as opposed to say a socket)
class NodeFile : public File {
public:
    static ErrorOr<NodeFile*> Create(class FsNode* ino);

    ~NodeFile() override;

    ErrorOr<ssize_t> Read(off_t off, size_t size, UIOBuffer* buffer) override;  // Read Data
    ErrorOr<ssize_t> Write(off_t off, size_t size, UIOBuffer* buffer) override; // Write Data
    ErrorOr<int> Ioctl(uint64_t cmd, uint64_t arg) override;
    ErrorOr<int> ReadDir(struct DirectoryEntry*, uint32_t) override;
    ErrorOr<class MappedRegion*> MMap(uintptr_t base, size_t size, off_t off, int prot, bool shared,
                                      bool fixed) override;
private:
    NodeFile() = default;
};
