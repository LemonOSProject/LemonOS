#include <Scheduler.h>
#include <Syscalls.h>

#include <Fs/EPoll.h>
#include <Net/Socket.h>

#include <UserPointer.h>
#include <OnCleanup.h>

long SysEpollCreate(RegisterContext* r) {
    Process* proc = Process::Current();
    int flags = SC_ARG0(r);

    if (flags & (~EPOLL_CLOEXEC)) {
        return -EINVAL;
    }

    fs::EPoll* ep = new fs::EPoll();
    FancyRefPtr<UNIXOpenFile> handle = SC_TRY_OR_ERROR(ep->Open(0));
    handle->node = ep;
    handle->mode = 0;
    if (flags & EPOLL_CLOEXEC) {
        handle->mode |= O_CLOEXEC;
    }

    int fd = proc->AllocateHandle(handle);
    return fd;
}

long SysEPollCtl(RegisterContext* r) {
    Process* proc = Process::Current();

    int epfd = SC_ARG0(r);
    int op = SC_ARG1(r);
    int fd = SC_ARG2(r);
    UserPointer<struct epoll_event> event = SC_ARG3(r);

    auto epHandle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(epfd));
    auto handle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(fd));

    if (!(handle && epHandle)) {
        return -EBADF; // epfd or fd is invalid
    }

    if (handle->node->IsEPoll()) {
        Log::Warning("SysEPollCtl: Currently watching other epoll devices is not supported.");
    }

    if (epfd == fd) {
        Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "epfd == fd");
        return -EINVAL;
    } else if (!epHandle->node->IsEPoll()) {
        Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "Not an epoll device!");
        return -EINVAL; // Not an epoll device
    }

    fs::EPoll* epoll = (fs::EPoll*)epHandle->node;

    ScopedSpinLock<true> lockEp(epoll->epLock);

    if (op == EPOLL_CTL_ADD) {
        struct epoll_event e;
        TRY_GET_UMODE_VALUE(event, e);

        if (e.events & EPOLLEXCLUSIVE) {
            // Unsupported
            Log::Warning("SysEPollCtl: EPOLLEXCLUSIVE unsupported!");
        }

        if (e.events & EPOLLET) {
            Log::Warning("SysEPollCtl: EPOLLET unsupported!");
        }

        for (const auto& pair : epoll->fds) {
            if (pair.item1 == fd) {
                return -EEXIST; // Already watching the fd
            }
        }

        epoll->fds.add_back({fd, {e.events, e.data}});
    } else if (op == EPOLL_CTL_DEL) {
        for (auto it = epoll->fds.begin(); it != epoll->fds.end(); it++) {
            if (it->item1 == fd) {
                epoll->fds.remove(it);
                return 0;
            }
        }

        return -ENOENT;
    } else if (op == EPOLL_CTL_MOD) {
        struct epoll_event* current = nullptr;
        struct epoll_event e;
        TRY_GET_UMODE_VALUE(event, e);

        if (e.events & EPOLLEXCLUSIVE) {
            // Not that we support this flag yet,
            // however for future it can only be
            // enabled on EPOLL_CTL_ADD.
            return -EINVAL;
        }

        for (auto& pair : epoll->fds) {
            if (pair.item1 == fd) {
                current = &pair.item2;
                break;
            }
        }

        if (!current) {
            return -ENOENT; // fd not found
        }
    }

    return -EINVAL;
}

long SysEpollWait(RegisterContext* r) {
    Process* proc = Process::Current();
    Thread* thread = Thread::Current();

    int epfd = SC_ARG0(r);
    UserBuffer<struct epoll_event> events = SC_ARG1(r);
    int maxevents = SC_ARG2(r);
    long timeout = SC_ARG3(r) * 1000;

    auto epHandle = SC_TRY_OR_ERROR(proc->GetHandleAs<UNIXOpenFile>(epfd));
    if (!epHandle) {
        return -EBADF;
    } else if (!epHandle->node->IsEPoll()) {
        Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "Not an epoll device!");
        return -EINVAL; // Not an epoll device
    }

    if (maxevents <= 0) {
        Log::Debug(debugLevelSyscalls, DebugLevelVerbose, "maxevents cannot be <= 0");
        return -EINVAL; // maxevents cannot be <= 0
    }

    fs::EPoll* epoll = (fs::EPoll*)epHandle->node;

    int64_t sigmask = SC_ARG4(r);
    int64_t oldsigmask = 0;
    if (sigmask) {
        oldsigmask = thread->signalMask;
        thread->signalMask = sigmask;
    }

    OnCleanup([sigmask, oldsigmask, thread]() {
        if (sigmask) {
            thread->signalMask = oldsigmask;
        }
    });

    ScopedSpinLock lockEp(epoll->epLock);

    Vector<int> removeFds; // Fds to unwatch
    auto getEvents = [removeFds](FancyRefPtr<UNIXOpenFile>& handle, uint32_t requested, int& evCount) -> uint32_t {
        uint32_t ev = 0;
        if (requested & EPOLLIN) {
            if (handle->node->CanRead()) {
                ev |= EPOLLIN;
            }
        }

        if (requested & EPOLLOUT) {
            if (handle->node->CanWrite()) {
                ev |= EPOLLOUT;
            }
        }

        if (handle->node->IsSocket()) {
            if (!((Socket*)handle->node)->IsConnected() && !((Socket*)handle->node)->IsListening()) {
                ev |= EPOLLHUP;
            }

            if (((Socket*)handle->node)->PendingConnections() && (requested & EPOLLIN)) {
                ev |= EPOLLIN;
            }
        }

        if (ev) {
            evCount++;
        }

        return ev;
    };

    auto epollToPollEvents = [](uint32_t ep) -> int {
        int evs = 0;
        if (ep & EPOLLIN) {
            evs |= POLLIN;
        }

        if (ep & EPOLLOUT) {
            evs |= POLLOUT;
        }

        if (ep & EPOLLHUP) {
            evs |= POLLHUP;
        }

        if (ep & EPOLLRDHUP) {
            evs |= POLLRDHUP;
        }

        if (ep & EPOLLERR) {
            evs |= POLLERR;
        }

        if (ep & EPOLLPRI) {
            evs |= POLLPRI;
        }

        return evs;
    };

    struct EPollFD {
        FancyRefPtr<UNIXOpenFile> handle;
        int fd;
        struct epoll_event ev;
    };

    Vector<EPollFD> files;
    int evCount = 0; // Amount of fds with events
    for (const auto& pair : epoll->fds) {
        auto result = proc->GetHandleAs<UNIXOpenFile>(pair.item1);
        if (result.HasError()) {
            continue; // Ignore closed fds
        }

        auto handle = std::move(result.Value());

        if (uint32_t ev = getEvents(handle, pair.item2.events, evCount); ev) {
            if (events.StoreValue(evCount - 1, {
                                                   .events = ev,
                                                   .data = pair.item2.data,
                                               })) {
                return -EFAULT;
            }
        }

        if (evCount > 0) {
            if (pair.item2.events & EPOLLONESHOT) {
                removeFds.add_back(pair.item1);
            }
        } else {
            files.add_back(
                {handle, pair.item1, pair.item2}); // We only need the handle for later if events were not found
        }

        if (evCount >= maxevents) {
            goto done; // Reached max events/fds
        }
    }

    if (evCount > 0) {
        goto done;
    }

    if (!evCount && timeout) {
        FilesystemWatcher fsWatcher;
        for (auto& file : files) {
            fsWatcher.WatchNode(file.handle->node, epollToPollEvents(file.ev.events));
        }

        while (!evCount) {
            if (timeout > 0) {
                if (fsWatcher.WaitTimeout(timeout)) {
                    return -EINTR; // Interrupted
                } else if (timeout <= 0) {
                    return 0; // Timed out
                }
            } else if (fsWatcher.Wait()) {
                return -EINTR; // Interrupted
            }

            for (auto& handle : files) {
                if (uint32_t ev = getEvents(handle.handle, handle.ev.events, evCount); ev) {
                    if (events.StoreValue(evCount - 1, {
                                                           .events = ev,
                                                           .data = handle.ev.data,
                                                       })) {
                        return -EFAULT;
                    }
                }

                if (evCount > 0) {
                    if (handle.ev.events & EPOLLONESHOT) {
                        removeFds.add_back(handle.fd);
                    }
                }

                if (evCount >= maxevents) {
                    goto done; // Reached max events/fds
                }
            }
        }
    }

done:
    for (int fd : removeFds) {
        for (auto it = epoll->fds.begin(); it != epoll->fds.end(); it++) {
            if (it->item1 == fd) {
                epoll->fds.remove(it);
                break;
            }
        }
    }

    return evCount;
}
