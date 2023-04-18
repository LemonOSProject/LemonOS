#include <Fs/Pipe.h>

#include <Move.h>
#include <Errno.h>

UNIXPipe::UNIXPipe(int _end, FancyRefPtr<DataStream> stream)
    : end(static_cast<decltype(end)>(_end)), stream(std::move(stream)){
}

ErrorOr<ssize_t> UNIXPipe::Read(size_t off, size_t size, UIOBuffer* buffer){
    if(end != ReadEnd){
        return Error{ESPIPE};
    }

    if(!widowed && size > stream->Pos()){
        FilesystemBlocker bl(this, size);

        if(Thread::current()->block(&bl)){
            return Error{EINTR};
        }
    }

    if(size > stream->Pos()){
        size = stream->Pos();
    }

    return stream->Read(buffer, size);
}

ErrorOr<ssize_t> UNIXPipe::Write(size_t off, size_t size, UIOBuffer* buffer){
    if(end != WriteEnd){
        return Error{ESPIPE};
    } else if(widowed || !otherEnd){
        Thread::current()->signal(SIGPIPE); // Send SIGPIPE on broken pipe
        return Error{EPIPE};
    }

    ssize_t ret = TRY_OR_ERROR(stream->Write(buffer, size));

    {
        ScopedSpinLock acq(otherEnd->watchingLock);

        for(auto& w : otherEnd->watching){
            w->Signal();
        }

        acquireLock(&otherEnd->blockedLock);
        FilesystemBlocker* bl = otherEnd->blocked.get_front();
        while(bl){
            FilesystemBlocker* next = otherEnd->blocked.next(bl);

            if(bl->RequestedLength() <= stream->Pos()){
                bl->Unblock();
            }

            bl = next;
        }
        releaseLock(&otherEnd->blockedLock);

        otherEnd->watching.clear();
    }

    return ret;
}

void UNIXPipe::Watch(FilesystemWatcher& watcher, int events){
    ScopedSpinLock acq(watchingLock);
    watching.add_back(&watcher);
}

void UNIXPipe::Unwatch(FilesystemWatcher& watcher){
    ScopedSpinLock acq(watchingLock);
    watching.remove(&watcher);
}

void UNIXPipe::Close(){
    handleCount--;

    if(handleCount <= 0){
        if(otherEnd){
            otherEnd->widowed = true;
        }

        delete this;
    }
}

void UNIXPipe::CreatePipe(UNIXPipe*& read, UNIXPipe*& write){
    FancyRefPtr<DataStream> stream = new DataStream(1024);
    
    read = new UNIXPipe(UNIXPipe::ReadEnd, stream);
    write = new UNIXPipe(UNIXPipe::WriteEnd, stream);

    read->otherEnd = write;
    write->otherEnd = read;
}