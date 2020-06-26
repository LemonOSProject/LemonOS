#include <net/socket.h>

#include <logging.h>
#include <assert.h>
#include <errno.h>
#include <scheduler.h>

Socket* Socket::CreateSocket(int domain, int type, int protocol){
    if(type & SOCK_NONBLOCK) type &= ~SOCK_NONBLOCK;

    if(domain != UnixDomain){
        Log::Warning("CreateSocket: domain %d is not supported", domain);
        return nullptr;
    }
    if(type != StreamSocket && type != DatagramSocket){
        Log::Warning("CreateSocket: type %d is not supported", type);
        return nullptr;
    }
    if(protocol){
        Log::Warning("CreateSocket: protocol is ignored");
    }
    
    if(domain == UnixDomain){
        return new LocalSocket(type, protocol);
    }

    return nullptr;
}

Socket::Socket(int type, int protocol){
    this->type = type;
    pendingConnections.value = CONNECTION_BACKLOG;
    flags = FS_NODE_SOCKET;
}

Socket::~Socket(){
    if(bound){
        SocketManager::DestroySocket(this);
    }
}

int Socket::ConnectTo(Socket* client){
    assert(!"ConnectTo has been called from socket base");

    return -1; // We should not return but get the compiler to shut up
}

Socket* Socket::Accept(sockaddr* addr, socklen_t* addrlen){
    return Accept(addr, addrlen, 0);
}

Socket* Socket::Accept(sockaddr* addr, socklen_t* addrlen, int mode){
    assert(!"Accept has been called from socket base");

    return nullptr;
}

int Socket::Bind(const sockaddr* addr, socklen_t addrlen){
    assert(!"Bind has been called from socket base");

    return -1;
}

int Socket::Connect(const sockaddr* addr, socklen_t addrlen){
    assert(!"Connect has been called from socket base");

    return -1;
}

int Socket::Listen(int backlog){
    return 0;
}

ssize_t Socket::Read(size_t offset, size_t size, uint8_t* buffer){
    return Receive(buffer, size, 0);
}

int64_t Socket::Receive(void* buffer, size_t len, int flags){
    if(!connected) ;

    return ReceiveFrom(buffer, len, flags, nullptr, nullptr);
}

int64_t Socket::ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen){
    assert(!"ReceiveFrom has been called from socket base");

    return -1; // We should not return but get the compiler to shut up
}

ssize_t Socket::Write(size_t offset, size_t size, uint8_t* buffer){
    return Send(buffer, size, 0);
}

int64_t Socket::Send(void* buffer, size_t len, int flags){
    return SendTo(buffer, len, flags, nullptr, 0);
}
    
int64_t Socket::SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen){
    assert(!"SendTo has been called from socket base");

    return -1; // We should not return but get the compiler to shut up
}

fs_fd_t* Socket::Open(size_t flags){
    fs_fd_t* fDesc = (fs_fd_t*)kmalloc(sizeof(fs_fd_t));

    fDesc->pos = 0;
    fDesc->mode = flags;
    fDesc->node = this;

    return fDesc;
}

void Socket::Close(){
    delete this;
}

LocalSocket::LocalSocket(int type, int protocol) : Socket(type, protocol){
    domain = UnixDomain;
    type = type;
    flags = FS_NODE_SOCKET;

    assert(type == StreamSocket || type == DatagramSocket);
}

int LocalSocket::ConnectTo(Socket* client){
    assert(passive);

    SemaphoreWait(&pendingConnections);

    pending.add_back(client);

    while(!client->connected){
        // TODO: Actually block the task
        Scheduler::Yield();
    }

    SemaphoreSignal(&pendingConnections);

    return 0;
}

Socket* LocalSocket::Accept(sockaddr* addr, socklen_t* addrlen, int mode){
    if((mode & O_NONBLOCK) && pending.get_length() <= 0){
        return nullptr;
    }

    while(pending.get_length() <= 0){
        // TODO: Actually block the task
        Scheduler::Yield();
    }

    Socket* next = pending.remove_at(0);
    assert(next->GetDomain() == UnixDomain);

    LocalSocket* client = (LocalSocket*) next;

    LocalSocket* sock = new LocalSocket(*client);
    sock->outbound = client->inbound; // Outbound to client
    sock->inbound = client->outbound; // Inbound to server
    sock->role = ServerRole;
    sock->peer = client;
    client->peer = sock;

    sock->connected = client->connected = true;

    return sock;
}

int LocalSocket::Bind(const sockaddr* addr, socklen_t addrlen){
    if(addr->family != UnixDomain){
        Log::Info("Bind: Socket is not a UNIX domain socket");
        return -EAFNOSUPPORT;
    }

    if(addrlen > sizeof(sockaddr_un)){
        Log::Info("Bind: Invalid address length");
        return -EINVAL;
    }

    if(bound){
        Log::Info("Bind: Socket already bound");
        return -EINVAL;
    }

    if(SocketManager::BindSocket(this, addr, addrlen)) {
        Log::Info("Bind: Address in use");
        return -EADDRINUSE;
    }

    bound = true;

    return 0;
}

int LocalSocket::Connect(const sockaddr* addr, socklen_t addrlen){
    if(addr->family != UnixDomain){
        Log::Info("Connect: Socket is not a UNIX domain socket");
        return -EAFNOSUPPORT;
    }

    if(addrlen > sizeof(sockaddr_un)){
        Log::Info("Connect: Invalid address length");
        return -EINVAL;
    }

    if(connected){
        Log::Info("Connect: Already connected");
        return -EISCONN;
    }

    if(passive){
        Log::Info("Connect: Cannot connect on a listen socket");
        return -EOPNOTSUPP;
    }

    if(type == StreamSocket){
        inbound = new DataStream(1024);
        outbound = new DataStream(1024);
    } else if (type == DatagramSocket){
        inbound = new PacketStream();
        outbound = new PacketStream();
    }

    role = ClientRole;

    Socket* sock = SocketManager::ResolveSocketAddress(addr, addrlen);
    if(!(sock && sock->IsListening())){ // Make sure the socket is both present and listening
        Log::Warning("Socket Not Found");
        return -ECONNREFUSED;
    }

    sock->ConnectTo(this);

    return 0;
}

int LocalSocket::Listen(int backlog){
    pendingConnections.value = backlog;
    passive = true;

    if(inbound) {
        delete inbound;
        inbound = nullptr;
    }

    if(outbound) {
        delete outbound;
        outbound = nullptr;
    }

    return 0;
}

int64_t LocalSocket::ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen){
    if(type == StreamSocket){
        if(src || addrlen){
            return -EISCONN;
        }
    } else if (type == DatagramSocket){
        if(src || addrlen){
            return -EOPNOTSUPP;
        }
    }

    if(!inbound){
        Log::Warning("LocalSocket not connected");
        return -ENOTCONN;
    }

    if(inbound->Empty() && (flags & MSG_DONTWAIT)){
        return -EAGAIN;
    }

    while(inbound->Empty()){
        // TODO: Actually block
        Scheduler::Yield();
    }

    if(flags & MSG_PEEK){
        return inbound->Peek(buffer, len);
    } else {
        return inbound->Read(buffer, len);
    }
}

int64_t LocalSocket::SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen){
    if(type == StreamSocket){
        if(src || addrlen){
            return -EISCONN;
        }
    } else if (type == DatagramSocket){
        if(src || addrlen){
            return -EOPNOTSUPP;
        }
    }

    if(!connected){
        Log::Warning("LocalSocket not connected");
        return -ENOTCONN;
    }

    return outbound->Write(buffer, len);
}

fs_fd_t* LocalSocket::Open(size_t flags){
    fs_fd_t* fDesc = (fs_fd_t*)kmalloc(sizeof(fs_fd_t));

    fDesc->pos = 0;
    fDesc->mode = flags;
    fDesc->node = this;

    return fDesc;
}

void LocalSocket::Close(){
    if(peer){
        peer->connected = false;
    }

    Socket::Close();
}

namespace SocketManager{
    List<SocketBinding> sockets;

    Socket* ResolveSocketAddress(const sockaddr* addr, socklen_t addrlen){
        char* address = (char*)kmalloc(addrlen + 1);
        strncpy(address, addr->data, addrlen);
        address[addrlen] = 0;

        for(unsigned i = 0; i < sockets.get_length(); i++){
            if(strcmp(sockets.get_at(i).address, address) == 0){
                Log::Info("Resolved Socket Address %s", address);

                return sockets.get_at(i).socket;
            }
        }

        return nullptr;
    }

    int BindSocket(Socket* sock, const sockaddr* addr, socklen_t addrlen){
        char* address = (char*)kmalloc(addrlen + 1);
        strncpy(address, addr->data, addrlen);
        address[addrlen] = 0;

        for(unsigned i = 0; i < sockets.get_length(); i++){
            if(strcmp(sockets.get_at(i).address, address) == 0) {
                kfree(address);
                return -1; // Address in use
            }
        }

        SocketBinding binding;
        binding.address = address;
        binding.socket = sock;

        sockets.add_back(binding);

        return 0;
    }

    void DestroySocket(Socket* sock){
        for(unsigned i = 0; i < sockets.get_length(); i++){
            if(sockets[i].socket == sock) {
                kfree(sockets[i].address);
                sockets.remove_at(i);
            }
        }
    }
}