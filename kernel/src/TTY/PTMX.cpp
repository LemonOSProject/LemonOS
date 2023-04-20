#include <TTY/PTY.h>

PTMultiplexor* PTMultiplexor::m_instance;

void PTMultiplexor::Initialize(){
    m_instance = new PTMultiplexor();
}

PTMultiplexor::PTS::PTS(PTMultiplexor& ptmx)
    : Device("pts", DeviceTypeUNIXPseudo), m_ptmx(ptmx) {
    type = FileType::Directory;
    pmask = 0755;
}

ErrorOr<int> PTMultiplexor::PTS::read_dir(DirectoryEntry* dirent, uint32_t index){
    ScopedSpinLock lock(m_ptmx.m_ptmxLock);
    if(index >= m_ptmx.m_ptList.get_length()){
        return 0;
    }

    *dirent = m_ptmx.m_ptList[index]->slaveDirent;
    return 1;
}

ErrorOr<FsNode*> PTMultiplexor::PTS::find_dir(const char* name){
    ScopedSpinLock lock(m_ptmx.m_ptmxLock);
    for(auto& pty : m_ptmx.m_ptList){
        if(!strcmp(pty->slaveDirent.name, name)){
            return pty;
        }
    }

    return nullptr;
}

Error PTMultiplexor::PTS::create(DirectoryEntry*, uint32_t){
    return Error{EPERM};
}

Error PTMultiplexor::PTS::create_directory(DirectoryEntry*, uint32_t){
    return Error{EPERM};
}

Error PTMultiplexor::PTS::link(FsNode*, DirectoryEntry*){
    return Error{EPERM};
}

Error PTMultiplexor::PTS::unlink(DirectoryEntry*, bool){
    return Error{EPERM};
}

PTMultiplexor::PTMultiplexor()
    : Device("ptmx", DeviceTypeUNIXPseudo), m_pts(*this) {
    type = FileType::CharDevice;
    pmask = 0666;
    uid = 0;

    SetDeviceName("UNIX PTY Multiplexor");
}

ErrorOr<File*> PTMultiplexor::Open(size_t flags){
    PTY* pty = new PTY(m_nextPT++);

    ScopedSpinLock lock(m_ptmxLock);
    m_ptList.add_back(pty);

    return pty->Open(flags);
}

void PTMultiplexor::DestroyPTY(PTY* pt){
    ScopedSpinLock lock(m_ptmxLock);
    for(auto it = m_ptList.begin(); it != m_ptList.end(); it++) {
        if(*it == pt){
            m_ptList.remove(pt);
            delete pt;

            return;
        }
    }
}
