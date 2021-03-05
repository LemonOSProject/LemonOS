#pragma once

#include <CharacterBuffer.h>
#include <Types.h>
#include <Scheduler.h>
#include <Device.h>

typedef unsigned int cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define TIOCGATTR 0xB301
#define TIOCSATTR 0xB302
#define TIOCFLUSH 0xB303
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414

#define NCCS    11
#define VEOF     0
#define VEOL     1
#define VERASE   2
#define VINTR    3
#define VKILL    4
#define VMIN     5
#define VQUIT    6
#define VSTART   7
#define VSTOP    8
#define VSUSP    9
#define VTIME   10

// c_iflag
#define BRKINT  0x0001
#define ICRNL   0x0002
#define IGNBRK  0x0004
#define IGNCR   0x0008
#define IGNPAR  0x0010
#define INLCR   0x0020
#define INPCK   0x0040
#define ISTRIP  0x0080
#define IXANY   0x0100
#define IXOFF   0x0200
#define IXON    0x0400
#define PARMRK  0x0800

// Flush constants
#define TCIFLUSH 1
#define TCIOFLUSH 2
#define TCOFLUSH 3

// c_lflag
#define ECHO    0x0001
#define ECHOE   0x0002
#define ECHOK   0x0004
#define ECHONL  0x0008
#define ICANON  0x0010
#define IEXTEN  0x0020
#define ISIG    0x0040
#define NOFLSH  0x0080
#define TOSTOP  0x0100

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[NCCS];
	speed_t ibaud;
	speed_t obaud;
};

struct winsz {
	unsigned short rowCount; // Rows (in characters)
	unsigned short colCount; // Columns (in characters)
	unsigned short width; // Width (in pixels)
	unsigned short height; // Height (in pixels)
};

enum {
    PTYSlaveDevice,
    PTYMasterDevice,
};

class PTY;

class PTYDevice : public Device {
public:
    DirectoryEntry dirent;

    PTY* pty;

    int device;

    PTYDevice(const char* name);

    ssize_t Read(size_t, size_t, uint8_t *);
    ssize_t Write(size_t, size_t, uint8_t *);
    int Ioctl(uint64_t cmd, uint64_t arg);

    void Watch(FilesystemWatcher& watcher, int events);
    void Unwatch(FilesystemWatcher& watcher);

    bool CanRead();
};

class PTY{
private:
    List<FilesystemWatcher*> watchingSlave;
    List<FilesystemWatcher*> watchingMaster;
public:
    CharacterBuffer master;
    CharacterBuffer slave;

    PTYDevice masterFile;
    PTYDevice slaveFile;

    winsz wSz;
    termios tios;

    PTY(const char* name);

    bool Echo() { return tios.c_lflag & ECHO; }
    bool IsCanonical() { return tios.c_lflag & ICANON; }
    
    void UpdateLineCount();

    ssize_t Master_Read(char* buffer, size_t count);
    ssize_t Slave_Read(char* buffer, size_t count);

    ssize_t Master_Write(char* buffer, size_t count);
    ssize_t Slave_Write(char* buffer, size_t count);

    void WatchMaster(FilesystemWatcher& watcher, int events);
    void WatchSlave(FilesystemWatcher& watcher, int events);
    void UnwatchMaster(FilesystemWatcher& watcher);
    void UnwatchSlave(FilesystemWatcher& watcher);
};

PTY* GrantPTY(uint64_t pid);