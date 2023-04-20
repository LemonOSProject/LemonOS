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
    ErrorOr<ssize_t> read(size_t size, UIOBuffer* buffer);                    // Read Data
    virtual ErrorOr<ssize_t> read(off_t off, size_t size, UIOBuffer* buffer); // Read Data

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
    ErrorOr<ssize_t> write(size_t size, UIOBuffer* buffer);                    // Write Data
    virtual ErrorOr<ssize_t> write(off_t off, size_t size, UIOBuffer* buffer); // Write Data

    virtual ErrorOr<int> ioctl(uint64_t cmd, uint64_t arg);

    // Read Directory
    virtual ErrorOr<int> read_dir(struct DirectoryEntry*, uint32_t);

    virtual ErrorOr<class MappedRegion*> mmap(uintptr_t base, size_t size, off_t off, int prot, bool shared,
                                              bool fixed);

    virtual void Watch(class KernelObjectWatcher* watcher, KOEvent events) override;
    virtual void Unwatch(class KernelObjectWatcher* watcher) override;

    ALWAYS_INLINE bool is_file() const { return type == FileType::Regular; }
    ALWAYS_INLINE bool is_block_dev() const { return type == FileType::BlockDevice; }
    ALWAYS_INLINE bool is_char_dev() const { return type == FileType::CharDevice; }
    ALWAYS_INLINE bool is_pipe() const { return type == FileType::Pipe; }
    ALWAYS_INLINE bool is_directory() const { return type == FileType::Directory; }
    ALWAYS_INLINE bool is_symlink() const { return type == FileType::SymbolicLink; }
    ALWAYS_INLINE bool is_socket() const { return type == FileType::Socket; }

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

    ErrorOr<ssize_t> read(off_t off, size_t size, UIOBuffer* buffer) override;  // Read Data
    ErrorOr<ssize_t> write(off_t off, size_t size, UIOBuffer* buffer) override; // Write Data
    ErrorOr<int> ioctl(uint64_t cmd, uint64_t arg) override;
    ErrorOr<int> read_dir(struct DirectoryEntry*, uint32_t) override;
    ErrorOr<class MappedRegion*> mmap(uintptr_t base, size_t size, off_t off, int prot, bool shared,
                                      bool fixed) override;
private:
    NodeFile() = default;
};
