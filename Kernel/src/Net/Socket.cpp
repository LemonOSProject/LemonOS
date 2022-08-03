#include <Net/Socket.h>

#include <Assert.h>
#include <Errno.h>
#include <Logging.h>
#include <Scheduler.h>

int Socket::CreateSocket(int domain, int type, int protocol, Socket** sock) {
    if (type & SOCK_NONBLOCK)
        type &= ~SOCK_NONBLOCK;

    if (domain != UnixDomain && domain != InternetProtocol) {
        Log::Warning("CreateSocket: domain %d is not supported", domain);
        return -EAFNOSUPPORT;
    }
    if (type != StreamSocket && type != DatagramSocket) {
        Log::Warning("CreateSocket: type %d is not supported", type);
        return -EPROTONOSUPPORT;
    }

    if (protocol) {
        Log::Warning("CreateSocket: protocol is ignored");
    }

    if (domain == UnixDomain) {
        *sock = new LocalSocket(type, protocol);
        return 0;
    } else if (domain == InternetProtocol) {
        if (type == DatagramSocket) {
            *sock = new Network::UDP::UDPSocket(type, protocol);
            return 0;
        } else if (type == StreamSocket) {
            *sock = new Network::TCP::TCPSocket(type, protocol);
            return 0;
        }
    }

    return -EINVAL;
}

Socket::Socket(int type, int protocol) {
    this->type = type;
    flags = FS_NODE_SOCKET;
}

Socket::~Socket() {}

Socket* Socket::Accept(sockaddr* addr, socklen_t* addrlen) { return Accept(addr, addrlen, 0); }

Socket* Socket::Accept(sockaddr* addr, socklen_t* addrlen, int mode) {
    assert(!"Accept has been called from socket base");

    return nullptr;
}

int Socket::Bind(const sockaddr* addr, socklen_t addrlen) {
    assert(!"Bind has been called from socket base");

    return -1;
}

int Socket::Connect(const sockaddr* addr, socklen_t addrlen) {
    assert(!"Connect has been called from socket base");

    return -1;
}

int Socket::Listen(int backlog) {
    return -EOPNOTSUPP; // If Listen has not been implemented assume that the socket is not connection-oriented
}

ssize_t Socket::Read(size_t offset, size_t size, uint8_t* buffer) { return Receive(buffer, size, 0); }

int64_t Socket::Receive(void* buffer, size_t len, int flags) {
    return ReceiveFrom(buffer, len, flags, nullptr, nullptr);
}

int64_t Socket::ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen,
                            const void* ancillary, size_t ancillaryLen) {
    assert(!"ReceiveFrom has been called from socket base");

    return -1; // We should not return but get the compiler to shut up
}

ssize_t Socket::Write(size_t offset, size_t size, uint8_t* buffer) { return Send(buffer, size, 0); }

int64_t Socket::Send(void* buffer, size_t len, int flags) { return SendTo(buffer, len, flags, nullptr, 0); }

int64_t Socket::SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen,
                       const void* ancillary, size_t ancillaryLen) {
    assert(!"SendTo has been called from socket base");

    return -1; // We should not return but get the compiler to shut up
}

int Socket::GetSocketOptions(int level, int opt, void* optValue, socklen_t* optLength) {
    if (level == SOL_SOCKET) {
        switch (opt) {
        case SO_ERROR:
            if (*optLength < sizeof(int)) {
                return -EFAULT; // Not big enough, callers job to check the memory space
            }

            *optLength = sizeof(int);
            *reinterpret_cast<int*>(optValue) = 0;

            Log::Warning("TCPSocket::GetSocketOptions: SO_ERROR is always 0");
            return 0;
        default:
            Log::Warning("TCPSocket::GetSocketOptions: Unknown option: %d", opt);
            return -ENOPROTOOPT;
        }
    }

    Log::Warning("Socket::GetSocketOptions: Unknown socket option %d at level %d!", opt, level);
    return -ENOPROTOOPT;
}

int Socket::SetSocketOptions(int level, int opt, const void* optValue, socklen_t optLength) {
    Log::Warning("Socket::SetSocketOptions: Unknown socket option %d at level %d!", opt, level);
    return -ENOPROTOOPT;
}

ErrorOr<UNIXOpenFile*> Socket::Open(size_t flags) {
    UNIXOpenFile* fDesc = new UNIXOpenFile();

    fDesc->pos = 0;
    fDesc->mode = flags;
    fDesc->node = this;

    handleCount++;

    return fDesc;
}

void Socket::Close() {}

void Socket::Watch(FilesystemWatcher& watcher, int events) { assert(!"Socket::Watch called from socket base"); }

void Socket::Unwatch(FilesystemWatcher& watcher) { assert(!"Socket::Unwatch called from socket base"); }

LocalSocket::LocalSocket(int type, int protocol) : Socket(type, protocol) {
    domain = UnixDomain;
    flags = FS_NODE_SOCKET;

    assert(type == StreamSocket || type == DatagramSocket);

    if (type == DatagramSocket) {
        inbound = new PacketStream();
        outbound = new PacketStream();
    } else {
        inbound = new DataStream(1024);
        outbound = new DataStream(1024);
    }
}

LocalSocket::~LocalSocket() {
    assert(!bound);
    assert(!m_connected);
    assert(!peer);

    if(inbound){
        delete inbound;
    }

    if(outbound){
        delete outbound;
    }
}

LocalSocket* LocalSocket::CreatePairedSocket(LocalSocket* client) {
    LocalSocket* sock = new LocalSocket(client->type, 0);

    if (sock->outbound) {
        delete sock->outbound;
    }

    if (sock->inbound) {
        delete sock->inbound;
    }

    sock->outbound = client->inbound; // Outbound to client
    sock->inbound = client->outbound; // Inbound to server
    sock->peer = client;
    client->peer = sock;

    sock->m_connected = client->m_connected = true;

    return sock;
}

int LocalSocket::ConnectTo(LocalSocket* client) {
    assert(passive);

    if (pendingConnections.Wait()) {
        return -EINTR;
    }

    pending.add_back(client);

    acquireLock(&m_watcherLock);
    while (m_watching.get_length()) {
        m_watching.remove_at(0)->Signal();
    }
    releaseLock(&m_watcherLock);

    while (!client->m_connected) {
        // TODO: Actually block the task
        Scheduler::Yield();
    }

    pendingConnections.Signal();

    return 0;
}

void LocalSocket::DisconnectPeer() {
    assert(peer);

    peer->OnDisconnect();

    peer = nullptr;
}

void LocalSocket::OnDisconnect() {
    m_connected = false;

    acquireLock(&m_watcherLock);
    while (m_watching.get_length()) {
        m_watching.remove_at(0)->Signal(); // Signal all watching on disconnect
    }
    releaseLock(&m_watcherLock);

    peer = nullptr;
}

Socket* LocalSocket::Accept(sockaddr* addr, socklen_t* addrlen, int mode) {
    if (!IsListening()) {
        return nullptr;
    }

    if ((mode & O_NONBLOCK) && pending.get_length() <= 0) {
        return nullptr;
    }

    while (pending.get_length() <= 0) {
        // TODO: Actually block the task
        // Log::Info("polling");
        Scheduler::Yield();
    }

    Socket* next = pending.remove_at(0);
    assert(next->GetDomain() == UnixDomain);

    LocalSocket* client = (LocalSocket*)next;

    LocalSocket* sock = CreatePairedSocket(client);
    return sock;
}

int LocalSocket::Bind(const sockaddr* addr, socklen_t addrlen) {
    if (addr->family != UnixDomain) {
        Log::Info("Bind: Socket is not a UNIX domain socket");
        return -EAFNOSUPPORT;
    }

    if (addrlen > sizeof(sockaddr_un)) {
        Log::Info("Bind: Invalid address length");
        return -EINVAL;
    }

    if (bound) {
        Log::Info("Bind: Socket already bound");
        return -EINVAL;
    }

    const sockaddr_un* unixAddress = reinterpret_cast<const sockaddr_un*>(addr);
    FsNode* parent = fs::ResolveParent(unixAddress->sun_path);

    if (!parent) {
        Log::Warning("LocalSocket::Bind: Parent directory for '%s' does not exist!", unixAddress->sun_path);
        return -ENOENT;
    }

    String name = fs::BaseName(unixAddress->sun_path);
    DirectoryEntry ent(this, name.c_str());
    if (int e = fs::Link(parent, this, &ent); e) {
        return e;
    }

    m_binding = *unixAddress;
    bound = true;

    return 0;
}

int LocalSocket::Connect(const sockaddr* addr, socklen_t addrlen) {
    if (addr->family != UnixDomain) {
        Log::Info("Connect: Socket is not a UNIX domain socket");
        return -EAFNOSUPPORT;
    }

    if (addrlen > sizeof(sockaddr_un)) {
        Log::Info("Connect: Invalid address length");
        return -EINVAL;
    }

    if (m_connected) {
        Log::Info("Connect: Already connected");
        return -EISCONN;
    }

    if (passive) {
        Log::Info("Connect: Cannot connect on a listen socket");
        return -EOPNOTSUPP;
    }

    ScopedSpinLock acquired(m_slock);
    FsNode* node = fs::ResolvePath(reinterpret_cast<const sockaddr_un*>(addr)->sun_path);
    if (!node->IsSocket()) {
        Log::Info("LocalSocket::Connect: Not a socket");
        return -ENOTSOCK;
    }

    Socket* sock = reinterpret_cast<Socket*>(node);
    if (!(sock && sock->IsListening())) { // Make sure the socket is both present and listening
        Log::Warning("Socket Not Found");
        return -ECONNREFUSED;
    }

    if (sock->GetDomain() != UnixDomain || sock->GetType() != type) {
        Log::Warning("Socket is not a UNIX Domain Socket of same type!");
        return -ENOSYS;
    }

    return ((LocalSocket*)sock)->ConnectTo(this);
}

int LocalSocket::Listen(int backlog) {
    ScopedSpinLock acquired(m_slock);
    pendingConnections.SetValue(backlog + 1);
    passive = true;

    if (inbound) {
        delete inbound;
        inbound = nullptr;
    }

    if (outbound) {
        delete outbound;
        outbound = nullptr;
    }

    releaseLock(&m_slock);
    return 0;
}

int64_t LocalSocket::ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen,
                                 const void* ancillary, size_t ancillaryLen) {
    if (type == StreamSocket) {
        if (src || addrlen) {
            return -EISCONN;
        }
    } else if (type == DatagramSocket) {
        if (src || addrlen) {
            return -EOPNOTSUPP;
        }
    }

    if (!inbound) {
        Log::Warning("LocalSocket::ReceiveFrom: LocalSocket not connected");
        return -ENOTCONN;
    }

    if (inbound->Empty() && (flags & MSG_DONTWAIT)) {
        return -EAGAIN;
    } else
        while (inbound->Empty()) {
            inbound->Wait();
        }

    if (flags & MSG_PEEK) {
        return inbound->Peek(buffer, len);
    } else {
        return inbound->Read(buffer, len);
    }
}

int64_t LocalSocket::SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen,
                            const void* ancillary, size_t ancillaryLen) {
    if (type == StreamSocket) {
        if (src || addrlen) {
            return -EISCONN;
        }
    } else if (type == DatagramSocket) {
        if (src || addrlen) {
            return -EOPNOTSUPP;
        }
    }

    if (!m_connected) {
        Log::Warning("LocalSocket::SendTo: LocalSocket not connected, peer set? %Y", peer);
        return -ENOTCONN;
    }

    if (type == StreamSocket && (flags & MSG_DONTWAIT) && outbound->Pos() + len >= STREAM_MAX_BUFSIZE) {
        return -EAGAIN;
    } else
        while (type == StreamSocket && outbound->Pos() + len >= STREAM_MAX_BUFSIZE) {
            Scheduler::Yield();
        }

    int64_t written = outbound->Write(buffer, len);

    if (peer && peer->CanRead()) {
        acquireLock(&peer->m_watcherLock);
        while (peer->m_watching.get_length()) {
            peer->m_watching.remove_at(0)->Signal();
        }
        releaseLock(&peer->m_watcherLock);
    }

    return written;
}

ErrorOr<UNIXOpenFile*> LocalSocket::Open(size_t flags) {
    UNIXOpenFile* fDesc = new UNIXOpenFile;

    fDesc->pos = 0;
    fDesc->mode = flags;
    fDesc->node = this;

    handleCount++;

    return fDesc;
}

void LocalSocket::Close() {
    if (handleCount == 0) {
        if (peer) {
            DisconnectPeer();
        }

        m_connected = false;

        if (bound) {
            DirectoryEntry ent(this, m_binding.sun_path);
            if(int e = fs::Unlink(this, &ent); e){
                assert(!e);
                Log::Error("LocalSocket::Close: Error unlinking bound socket!");
            }

            bound = false;
        }

        delete this;
    }
}

void LocalSocket::Watch(FilesystemWatcher& watcher, int events) {
    if (!(events & (POLLIN | POLLPRI))) {
        return;
    }

    if (CanRead() | !IsConnected()) { // POLLHUP does not care if it is requested
        return;
    }

    acquireLock(&m_watcherLock);
    m_watching.add_back(&watcher);
    releaseLock(&m_watcherLock);
}

void LocalSocket::Unwatch(FilesystemWatcher& watcher) {
    acquireLock(&m_watcherLock);
    m_watching.remove(&watcher);
    releaseLock(&m_watcherLock);
}
