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

ErrorOr<int> PTMultiplexor::PTS::ReadDir(DirectoryEntry* dirent, uint32_t index){
    ScopedSpinLock lock(m_ptmx.m_ptmxLock);
    if(index >= m_ptmx.m_ptList.get_length()){
        return 0;
    }

    *dirent = m_ptmx.m_ptList[index]->slaveDirent;
    return 1;
}

ErrorOr<FsNode*> PTMultiplexor::PTS::FindDir(const char* name){
    ScopedSpinLock lock(m_ptmx.m_ptmxLock);
    for(auto& pty : m_ptmx.m_ptList){
        if(!strcmp(pty->slaveDirent.name, name)){
            return &pty->slaveFile;
        }
    }

    return nullptr;
}

Error PTMultiplexor::PTS::Create(DirectoryEntry*, uint32_t){
    return Error{EPERM};
}

Error PTMultiplexor::PTS::CreateDirectory(DirectoryEntry*, uint32_t){
    return Error{EPERM};
}

Error PTMultiplexor::PTS::Link(FsNode*, DirectoryEntry*){
    return Error{EPERM};
}

Error PTMultiplexor::PTS::Unlink(DirectoryEntry*, bool){
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

    return pty->masterFile.Open(flags);
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
