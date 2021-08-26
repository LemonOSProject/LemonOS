#pragma once

#include <CharacterBuffer.h>
#include <Types.h>
#include <Scheduler.h>
#include <Device.h>
#include <List.h>

#include <ABI/termios.h>

enum {
    PTYSlaveDevice,
    PTYMasterDevice,
};

class PTY;

class PTMultiplexor final : public Device {
public:
    class PTS final : public Device {
    public:
        PTS(PTMultiplexor& ptmx);

        int ReadDir(DirectoryEntry*, uint32_t) override;
        FsNode* FindDir(const char* name) override;

        int Create(DirectoryEntry* ent, uint32_t mode) override;
        int CreateDirectory(DirectoryEntry* ent, uint32_t mode) override;

        int Link(FsNode*, DirectoryEntry*) override;
        int Unlink(DirectoryEntry*, bool unlinkDirectories = false) override;
    
    private:
        PTMultiplexor& m_ptmx;
    };

    static void Initialize();
    static inline PTMultiplexor& Instance() { return *m_instance; }

    UNIXFileDescriptor* Open(size_t flags) override;

private:
    static PTMultiplexor* m_instance;

    PTMultiplexor();

    lock_t m_ptmxLock;
    int m_nextPT = 0; // PTYs are named /dev/pts/0, 1, 2, ... 

    PTS m_pts;
    List<class PTY*> m_ptList;
};

class PTYDevice : public FsNode {
public:
    PTY* pty;
    int device;

    PTYDevice();

    ssize_t Read(size_t, size_t, uint8_t *) override;
    ssize_t Write(size_t, size_t, uint8_t *) override;
    int Ioctl(uint64_t cmd, uint64_t arg) override;

    void Watch(FilesystemWatcher& watcher, int events) override;
    void Unwatch(FilesystemWatcher& watcher) override;

    bool CanRead() override;
};

class PTY{
public:
    CharacterBuffer master;
    CharacterBuffer slave;

    PTYDevice masterFile;
    PTYDevice slaveFile;

    // /dev/pts/<id>
    DirectoryEntry slaveDirent;

    winsz wSz;
    termios tios;

    PTY(int id);

    inline int GetID() const { return m_id; }

    bool Echo() { return tios.c_lflag & ECHO; }
    bool IsCanonical() { return tios.c_lflag & ICANON; }
    
    void UpdateLineCount();

    ssize_t MasterRead(char* buffer, size_t count);
    ssize_t SlaveRead(char* buffer, size_t count);

    ssize_t MasterWrite(char* buffer, size_t count);
    ssize_t SlaveWrite(char* buffer, size_t count);

    void WatchMaster(FilesystemWatcher& watcher, int events);
    void WatchSlave(FilesystemWatcher& watcher, int events);
    void UnwatchMaster(FilesystemWatcher& watcher);
    void UnwatchSlave(FilesystemWatcher& watcher);
private:
    int m_id;

    List<FilesystemWatcher*> m_watchingSlave;
    List<FilesystemWatcher*> m_watchingMaster;
};

PTY* GrantPTY(uint64_t pid);