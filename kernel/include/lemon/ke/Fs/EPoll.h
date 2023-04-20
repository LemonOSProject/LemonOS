#pragma once

#include <abi/EPoll.h>
#include <Fs/Filesystem.h>
#include <List.h>
#include <Pair.h>

namespace fs {

class EPoll :
    public FsNode {
public:
    EPoll() = default;

    void Close() override {
        handleCount--;

        if(handleCount <= 0){
            delete this;
        }
    }

    bool is_epoll() const override {
        return true;
    }

    List<Pair<int, epoll_event>> fds;
    lock_t epLock = 0;
};

}
