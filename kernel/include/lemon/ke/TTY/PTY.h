#pragma once

#include <CharacterBuffer.h>
#include <Types.h>
#include <Scheduler.h>
#include <Device.h>
#include <List.h>

#include <abi/termios.h>

enum class PTYType {
    Slave,
    Master,
};

class PTY;

class PTMultiplexor final : public Device {
public:
    class PTS final : public Device {
    public:
        PTS(PTMultiplexor& ptmx);

        ErrorOr<int> ReadDir(DirectoryEntry*, uint32_t) override;
        ErrorOr<FsNode*> FindDir(const char* name) override;

        Error Create(DirectoryEntry* ent, uint32_t mode) override;
        Error CreateDirectory(DirectoryEntry* ent, uint32_t mode) override;

        Error Link(FsNode*, DirectoryEntry*) override;
        Error Unlink(DirectoryEntry*, bool unlinkDirectories = false) override;
    
    private:
        PTMultiplexor& m_ptmx;
    };

    static void Initialize();
    static inline PTMultiplexor& Instance() { return *m_instance; }

    ErrorOr<File*> Open(size_t flags) override;
    void DestroyPTY(PTY* pt);

private:
    static PTMultiplexor* m_instance;

    PTMultiplexor();

    lock_t m_ptmxLock = 0;
    int m_nextPT = 0; // PTYs are named /dev/pts/0, 1, 2, ... 

    PTS m_pts;
    List<class PTY*> m_ptList;
};

class PTYDevice : public File {
public:
    PTYDevice(PTYType dType);
    ~PTYDevice() override;

    ErrorOr<ssize_t> Read(off_t off, size_t size, UIOBuffer* buffer) override;
    ErrorOr<ssize_t> Write(off_t off, size_t size, UIOBuffer* buffer) override;
    ErrorOr<int> Ioctl(uint64_t cmd, uint64_t arg) override;

    void Watch(class KernelObjectWatcher* watcher, KOEvent events) override;
    void Unwatch(class KernelObjectWatcher* watcher) override;

    bool CanRead();

    PTYType ptyType;
    PTY* pty;
};

class PTY : public FsNode {
public:
    CharacterBuffer master;
    CharacterBuffer slave;

    FancyRefPtr<PTYDevice> masterFile;
    FancyRefPtr<PTYDevice> slaveFile;

    // /dev/pts/<id>
    DirectoryEntry slaveDirent;

    winsz wSz;
    termios tios;

    PTY(int id);
    ~PTY();

    inline int GetID() const { return m_id; }

    bool Echo() { return tios.c_lflag & ECHO; }
    bool IsCanonical() { return tios.c_lflag & ICANON; }
    
    void UpdateLineCount();

    ErrorOr<ssize_t> MasterRead(UIOBuffer* buffer, size_t count);
    ErrorOr<ssize_t> SlaveRead(UIOBuffer* buffer, size_t count);

    ErrorOr<ssize_t> MasterWrite(UIOBuffer* buffer, size_t count);
    ErrorOr<ssize_t> SlaveWrite(UIOBuffer* buffer, size_t count);

    void Close();

    void WatchMaster(KernelObjectWatcher* watcher, KOEvent events);
    void WatchSlave(KernelObjectWatcher* watcher, KOEvent events);
    void UnwatchMaster(KernelObjectWatcher* watcher);
    void UnwatchSlave(KernelObjectWatcher* watcher);
private:
    int m_id;

    lock_t m_watcherLock = 0;
    List<KernelObjectWatcher*> m_watchingSlave;
    List<KernelObjectWatcher*> m_watchingMaster;
};

PTY* GrantPTY(uint64_t pid);