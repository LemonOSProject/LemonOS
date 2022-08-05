#include <Assert.h>
#include <CPU.h>
#include <CString.h>
#include <Errno.h>
#include <Fs/Filesystem.h>
#include <List.h>
#include <Logging.h>
#include <TTY/PTY.h>
#include <Scheduler.h>
#include <Types.h>

cc_t c_cc_default[NCCS]{
    4,    // VEOF
    '\n', // VEOL
    '\b', // VERASE
    0,    // VINTR
    0,    // VKILL
    0,    // VMIN
    0,    // VQUIT
    0,    // VSTART
    0,    // VSTOP
    0,    // VSUSP
    0,    // VTIME
};

PTYDevice::PTYDevice() {

}

ssize_t PTYDevice::Read(size_t offset, size_t size, uint8_t* buffer) {
    assert(pty);
    assert(device == PTYSlaveDevice || device == PTYMasterDevice);

    if (pty && device == PTYSlaveDevice)
        return pty->SlaveRead((char*)buffer, size);
    else if (pty && device == PTYMasterDevice) {
        return pty->MasterRead((char*)buffer, size);
    } else {
        assert(!"PTYDevice::Read: PTYDevice is designated neither slave nor master");
    }

    return 0;
}

ssize_t PTYDevice::Write(size_t offset, size_t size, uint8_t* buffer) {
    assert(pty);
    assert(device == PTYSlaveDevice || device == PTYMasterDevice);

    if (pty && device == PTYSlaveDevice) {
        ssize_t written = pty->SlaveWrite((char*)buffer, size);

        if (written < 0 || written == size) {
            return written; // Check either for an error or if all bytes were written
        }

        // Buffer must be full so just keep trying
        buffer += written;
        while (written < size) {
            size_t ret = pty->SlaveWrite((char*)buffer, size - written);

            if (ret < 0) {
                return ret; // Error
            }

            written += ret;
            buffer += ret;
        }

        return written;
    } else if (pty && device == PTYMasterDevice) {
        ssize_t written = pty->MasterWrite((char*)buffer, size);

        if (written < 0 || written == size) {
            return written; // Check either for an error or if all bytes were written
        }

        // Buffer must be full so just keep trying
        buffer += written;
        while (written < size) {
            size_t ret = pty->MasterWrite((char*)buffer, size - written);

            if (ret < 0) {
                return ret; // Error
            }

            written += ret;
            buffer += ret;
        }

        return written;
    } else {
        assert(!"PTYDevice::Write: PTYDevice is designated neither slave nor master");
    }

    return 0;
}

int PTYDevice::Ioctl(uint64_t cmd, uint64_t arg) {
    assert(pty);

    switch (cmd) {
    case TCGETS:
        *((termios*)arg) = pty->tios;
        break;
    case TCSETS:
        pty->tios = *((termios*)arg);
        pty->slave.ignoreBackspace = !pty->IsCanonical();
        break;
    case TIOCGWINSZ:
        *((winsz*)arg) = pty->wSz;
        break;
    case TIOCSWINSZ:
        pty->wSz = *((winsz*)arg);
        break;
    case TIOCGPTN:
        *((int*)arg) = pty->GetID();
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

void PTYDevice::Watch(FilesystemWatcher& watcher, int events) {
    if (device == PTYMasterDevice) {
        pty->WatchMaster(watcher, events);
    } else if (device == PTYSlaveDevice) {
        pty->WatchSlave(watcher, events);
    } else {
        assert(!"PTYDevice::Watch: PTYDevice is designated neither slave nor master");
    }
}

void PTYDevice::Unwatch(FilesystemWatcher& watcher) {
    if (device == PTYMasterDevice) {
        pty->UnwatchMaster(watcher);
    } else if (device == PTYSlaveDevice) {
        pty->UnwatchSlave(watcher);
    } else {
        assert(!"PTYDevice::Unwatch: PTYDevice is designated neither slave nor master");
    }
}

bool PTYDevice::CanRead() {
    if (device == PTYMasterDevice) {
        return !!pty->master.bufferPos;
    } else if (device == PTYSlaveDevice) {
        if (pty->IsCanonical())
            return !!pty->slave.lines;
        else
            return !!pty->slave.bufferPos;
    } else {
        assert(!"PTYDevice::CanRead: PTYDevice is designated neither slave nor master");
    }
}

void PTYDevice::Close() {
    handleCount--;

    if(handleCount == 0 && device == PTYMasterDevice){
        pty->Close();
    }
}

PTY::PTY(int id) : m_id(id) {
    slaveFile.flags = FS_NODE_CHARDEVICE;
    masterFile.flags = FS_NODE_CHARDEVICE;

    master.ignoreBackspace = true;
    slave.ignoreBackspace = false;
    master.Flush();
    slave.Flush();
    tios.c_lflag = ECHO | ICANON;

    strcpy(slaveDirent.name, to_string(id).c_str());
    slaveDirent.flags = DT_CHR;
    slaveDirent.inode = m_id;

    masterFile.pty = this;
    masterFile.device = PTYMasterDevice;
    masterFile.inode = m_id;

    slaveFile.pty = this;
    slaveFile.device = PTYSlaveDevice;
    slaveFile.inode = m_id;

    for (int i = 0; i < NCCS; i++)
        tios.c_cc[i] = c_cc_default[i];
}

PTY::~PTY(){
    while (m_watchingSlave.get_length()) {
        m_watchingSlave.remove_at(0)->Signal(); // Signal all watching
    }

    while (m_watchingMaster.get_length()) {
        m_watchingMaster.remove_at(0)->Signal(); // Signal all watching
    }
}

void PTY::Close() {
    // Only close this PTY if there are 0 open handles across both
    // the slave and master files
    if(masterFile.handleCount + slaveFile.handleCount == 0) {
        PTMultiplexor::Instance().DestroyPTY(this);
    }
}

ssize_t PTY::MasterRead(char* buffer, size_t count) { return master.Read(buffer, count); }

ssize_t PTY::SlaveRead(char* buffer, size_t count) {
    Thread* thread = Thread::Current();

    while (IsCanonical() && !slave.lines) {
        FilesystemBlocker blocker(&slaveFile);

        if (thread->Block(&blocker)) {
            return -EINTR;
        }
    }

    while (!IsCanonical() && !slave.bufferPos) {
        FilesystemBlocker blocker(&slaveFile);

        if (thread->Block(&blocker)) {
            return -EINTR;
        }
    }

    return slave.Read(buffer, count);
}

ssize_t PTY::MasterWrite(char* buffer, size_t count) {
    ssize_t ret = slave.Write(buffer, count);

    if (slaveFile.blocked.get_length()) {
        if (IsCanonical()) {
            while (slave.lines && slaveFile.blocked.get_length()) {
                acquireLock(&slaveFile.blockedLock);
                if (slaveFile.blocked.get_length()) {
                    slaveFile.blocked.get_front()->Unblock();
                }
                releaseLock(&slaveFile.blockedLock);
            }
        } else {
            while (slave.bufferPos && slaveFile.blocked.get_length()) {
                acquireLock(&slaveFile.blockedLock);
                if (slaveFile.blocked.get_length()) {
                    slaveFile.blocked.get_front()->Unblock();
                }
                releaseLock(&slaveFile.blockedLock);
            }
        }
    }

    if (Echo() && ret) {
        for (unsigned i = 0; i < count; i++) {
            if (buffer[i] == '\e') { // Escape
                master.Write("^[", 2);
            } else {
                master.Write(&buffer[i], 1);
            }
        }
    }

    if (IsCanonical()) {
        if (slave.lines && m_watchingSlave.get_length()) {
            while (m_watchingSlave.get_length()) {
                m_watchingSlave.remove_at(0)->Signal(); // Signal all watching
            }
        }
    } else {
        if (slave.bufferPos && m_watchingSlave.get_length()) {
            while (m_watchingSlave.get_length()) {
                m_watchingSlave.remove_at(0)->Signal(); // Signal all watching
            }
        }
    }

    return ret;
}

ssize_t PTY::SlaveWrite(char* buffer, size_t count) {
    ssize_t written = master.Write(buffer, count);

    while (master.bufferPos && masterFile.blocked.get_length()) {
        acquireLock(&masterFile.blockedLock);
        if (masterFile.blocked.get_length()) {
            masterFile.blocked.get_front()->Unblock();
        }
        releaseLock(&masterFile.blockedLock);
    }

    if (master.bufferPos && m_watchingMaster.get_length()) {
        while (m_watchingMaster.get_length()) {
            m_watchingMaster.remove_at(0)->Signal(); // Signal all watching
        }
    }

    return written;
}

void PTY::WatchMaster(FilesystemWatcher& watcher, int events) {
    if (!(events & (POLLIN))) { // We don't really block on writes and nothing else applies except POLLIN
        watcher.Signal();
        return;
    } else if (masterFile.CanRead()) {
        watcher.Signal();
        return;
    }

    m_watchingMaster.add_back(&watcher);
}

void PTY::WatchSlave(FilesystemWatcher& watcher, int events) {
    if (!(events & (POLLIN))) { // We don't really block on writes and nothing else applies except POLLIN
        watcher.Signal();
        return;
    } else if (slaveFile.CanRead()) {
        watcher.Signal();
        return;
    }

    m_watchingSlave.add_back(&watcher);
}

void PTY::UnwatchMaster(FilesystemWatcher& watcher) { m_watchingMaster.remove(&watcher); }

void PTY::UnwatchSlave(FilesystemWatcher& watcher) { m_watchingSlave.remove(&watcher); }