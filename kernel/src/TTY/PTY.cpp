#include <Assert.h>
#include <CPU.h>
#include <CString.h>
#include <Errno.h>
#include <Fs/Filesystem.h>
#include <List.h>
#include <Logging.h>
#include <Scheduler.h>
#include <TTY/PTY.h>
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

PTYDevice::PTYDevice(PTYType dType, PTY* pty) : ptyType(dType), pty(pty) {
    type = FileType::CharDevice;
}

PTYDevice::~PTYDevice() {
    if (pty) {
        pty->Close();
    }
}

ErrorOr<ssize_t> PTYDevice::read(off_t offset, size_t size, UIOBuffer* buffer) {
    assert(pty);

    if (pty && ptyType == PTYType::Slave)
        return pty->SlaveRead(buffer, size);
    else if (pty && ptyType == PTYType::Master) {
        return pty->MasterRead(buffer, size);
    } else {
        assert(!"PTYDevice::Read: PTYDevice is designated neither slave nor master");
    }

    return 0;
}

ErrorOr<ssize_t> PTYDevice::write(off_t offset, size_t size, UIOBuffer* buffer) {
    assert(pty);

    if (pty && ptyType == PTYType::Slave) {
        ssize_t written = TRY_OR_ERROR(pty->SlaveWrite(buffer, size));

        if (written < 0 || written == size) {
            return written; // Check either for an error or if all bytes were written
        }

        while (written < size) {
            buffer->set_offset(written);

            size_t ret = TRY_OR_ERROR(pty->SlaveWrite(buffer, size - written));

            written += ret;
            buffer += ret;
        }

        return written;
    } else if (pty && ptyType == PTYType::Master) {
        ssize_t written = TRY_OR_ERROR(pty->MasterWrite(buffer, size));

        if (written == size) {
            return written; // Check either for an error or if all bytes were written
        }

        while (written < size) {
            buffer->set_offset(written);

            size_t ret = TRY_OR_ERROR(pty->MasterWrite(buffer, size - written));

            written += ret;
            buffer += ret;
        }

        return written;
    } else {
        assert(!"PTYDevice::Write: PTYDevice is designated neither slave nor master");
    }

    return 0;
}

ErrorOr<int> PTYDevice::ioctl(uint64_t cmd, uint64_t arg) {
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
        return Error{EINVAL};
    }

    return 0;
}

KOEvent PTYDevice::poll_events(KOEvent mask) {
    KOEvent events = KOEvent::Out;
    if(pty->can_read(ptyType)) {
        events = events | KOEvent::In;
    }

    return KOEVENT_MASK(events, mask);
}

void PTYDevice::Watch(KernelObjectWatcher* watcher, KOEvent events) {
    if (ptyType == PTYType::Master) {
        pty->WatchMaster(watcher, events);
    } else if (ptyType == PTYType::Slave) {
        pty->WatchSlave(watcher, events);
    } else {
        assert(!"PTYDevice::Watch: PTYDevice is designated neither slave nor master");
    }
}

void PTYDevice::Unwatch(KernelObjectWatcher* watcher) {
    if (ptyType == PTYType::Master) {
        pty->UnwatchMaster(watcher);
    } else if (ptyType == PTYType::Slave) {
        pty->UnwatchSlave(watcher);
    } else {
        assert(!"PTYDevice::Unwatch: PTYDevice is designated neither slave nor master");
    }
}

PTY::PTY(int id) : m_id(id) {
    slaveFile = new PTYDevice(PTYType::Slave, this);
    masterFile = new PTYDevice(PTYType::Master, this);

    master.ignoreBackspace = true;
    slave.ignoreBackspace = false;
    master.flush();
    slave.flush();
    tios.c_lflag = ECHO | ICANON;

    strcpy(slaveDirent.name, to_string(id).c_str());
    slaveDirent.flags = DT_CHR;
    slaveDirent.ino = m_id;

    masterFile->pty = this;

    for (int i = 0; i < NCCS; i++)
        tios.c_cc[i] = c_cc_default[i];
}

PTY::~PTY() {
    while (m_watchingSlave.get_length()) {
        m_watchingSlave.remove_at(0)->signal(); // Signal all watching
    }

    while (m_watchingMaster.get_length()) {
        m_watchingMaster.remove_at(0)->signal(); // Signal all watching
    }
}

bool PTY::can_read(PTYType ptyType) {
    if (ptyType == PTYType::Master) {
        return !!master.bufferPos;
    } else if (ptyType == PTYType::Slave) {
        if (IsCanonical())
            return !!slave.lines;
        else
            return !!slave.bufferPos;
    } else {
        assert(!"PTY::can_read: PTYDevice is designated neither slave nor master");
    }
}

ErrorOr<class File*> PTY::OpenMaster(size_t flags) {
    ScopedSpinLock l(nodeLock);

    handleCount++;
    return new PTYDevice(PTYType::Master, this);
}

ErrorOr<class File*> PTY::Open(size_t flags) {
    ScopedSpinLock l(nodeLock);

    handleCount++;
    return new PTYDevice(PTYType::Slave, this);
}

void PTY::Close() {
    ScopedSpinLock l(nodeLock);

    // Only close this PTY if there are 0 open handles
    if (handleCount == 0) {
        PTMultiplexor::Instance().DestroyPTY(this);
    }
}

ErrorOr<ssize_t> PTY::MasterRead(UIOBuffer* buffer, size_t count) {
    char _buf[512];
    size_t bytesRead = 0;

    while(bytesRead < count) {
        ssize_t toRead = count - bytesRead;
        if(toRead > 512) {
            toRead = 512;
        }

        ssize_t r = master.read(_buf, toRead);
        if(buffer->write((uint8_t*)_buf, r)) {
            return EFAULT;
        }

        bytesRead += r;

        // Buffer might be empty, bytes read was under count
        if(r < toRead) {
            break;
        }
    }

    return bytesRead;
}

ErrorOr<ssize_t> PTY::SlaveRead(UIOBuffer* buffer, size_t count) {
    Thread* thread = Thread::current();

    while (IsCanonical() && !slave.lines) {
        KernelObjectWatcher w;
        w.Watch(slaveFile, KOEvent::In);

        if (w.wait()) {
            return Error{EINTR};
        }
    }

    while (!IsCanonical() && !slave.bufferPos) {
        KernelObjectWatcher w;
        w.Watch(slaveFile, KOEvent::In);

        if (w.wait()) {
            return Error{EINTR};
        }
    }
    
    char _buf[512];
    size_t bytesRead = 0;

    while(bytesRead < count) {
        ssize_t toRead = count - bytesRead;
        if(toRead > 512) {
            toRead = 512;
        }

        ssize_t r = slave.read(_buf, toRead);
        if(buffer->write((uint8_t*)_buf, r)) {
            return EFAULT;
        }

        bytesRead += r;

        // Buffer might be empty, bytes read was under count
        if(r < toRead) {
            break;
        }
    }

    return bytesRead;
}

ErrorOr<ssize_t> PTY::MasterWrite(UIOBuffer* buffer, size_t count) {
    char _buf[512];
    ssize_t written = 0;
    while(written < count) {
        if(buffer->read((uint8_t*)_buf, 512, written)) {
            return EFAULT;
        }

        ssize_t toWrite = 512;
        if(toWrite > count - written) {
            toWrite = count - written;
        }

        int r = slave.write(_buf, toWrite);
        written += r;
        
        // Buffer might be full, not all of our data was written
        if(r < toWrite) {
            break;
        }
    }

    if (Echo() && written) {
        for (unsigned i = 0; i < written; i++) {
            char c;
            if (buffer->read((uint8_t*)&c, 1, i)) {
                return EFAULT;
            }

            if (c == '\e') { // Escape
                master.write("^[", 2);
            } else {
                master.write(&c, 1);
            }
        }
    }

    ScopedSpinLock lockWatchers(m_watcherLock);
    if (IsCanonical()) {
        if (slave.lines && m_watchingSlave.get_length()) {
            while (m_watchingSlave.get_length()) {
                m_watchingSlave.remove_at(0)->signal(); // Signal all watching
            }
        }
    } else {
        if (slave.bufferPos && m_watchingSlave.get_length()) {
            while (m_watchingSlave.get_length()) {
                m_watchingSlave.remove_at(0)->signal(); // Signal all watching
            }
        }
    }

    return written;
}

ErrorOr<ssize_t> PTY::SlaveWrite(UIOBuffer* buffer, size_t count) {
    char _buf[512];
    ssize_t written = 0;
    while(written < count) {
        if(buffer->read((uint8_t*)_buf, 512, written)) {
            return EFAULT;
        }

        ssize_t toWrite = 512;
        if(toWrite > count - written) {
            toWrite = count - written;
        }

        int r = master.write(_buf, toWrite);
        written += r;
        
        // Buffer might be full, not all of our data was written
        if(r < toWrite) {
            break;
        }
    }

    ScopedSpinLock lockWatchers(m_watcherLock);
    if (master.bufferPos && m_watchingMaster.get_length()) {
        while (m_watchingMaster.get_length()) {
            m_watchingMaster.remove_at(0)->signal(); // Signal all watching
        }
    }

    return written;
}

void PTY::WatchMaster(KernelObjectWatcher* watcher, KOEvent events) {
    ScopedSpinLock lock{m_watcherLock};
    if (!HAS_KOEVENT(events, KOEvent::In)) { // We don't really block on writes and nothing else applies except POLLIN
        watcher->signal();
        return;
    } else if (can_read(PTYType::Master)) {
        watcher->signal();
        return;
    }

    m_watchingMaster.add_back(watcher);
}

void PTY::WatchSlave(KernelObjectWatcher* watcher, KOEvent events) {
    ScopedSpinLock lock{m_watcherLock};
    if (!HAS_KOEVENT(events, KOEvent::In)) { // We don't really block on writes and nothing else applies except POLLIN
        watcher->signal();
        return;
    } else if (can_read(PTYType::Slave)) {
        watcher->signal();
        return;
    }

    m_watchingSlave.add_back(watcher);
}

void PTY::UnwatchMaster(KernelObjectWatcher* watcher) {
    ScopedSpinLock lock{m_watcherLock};
    m_watchingMaster.remove(watcher);
}

void PTY::UnwatchSlave(KernelObjectWatcher* watcher) {
    ScopedSpinLock lock{m_watcherLock};
    m_watchingSlave.remove(watcher);
}
