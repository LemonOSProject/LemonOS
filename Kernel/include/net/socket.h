#pragma once

#include <stdint.h>
#include <stddef.h>
#include <fs/filesystem.h>
#include <lock.h>
#include <stream.h>

#define MSG_CTRUNC 0x1
#define MSG_DONTROUTE 0x2
#define MSG_EOR 0x4
#define MSG_OOB 0x8
#define MSG_NOSIGNAL 0x10
#define MSG_PEEK 0x20
#define MSG_TRUNC 0x40
#define MSG_WAITALL 0x80
#define MSG_DONTWAIT 0x1000

#define CONNECTION_BACKLOG 128

#define SOCK_NONBLOCK 0x10000

typedef unsigned int sa_family_t;
typedef uint32_t socklen_t;

typedef struct sockaddr {
    sa_family_t family;
    char data[14];
} sockaddr_t;

struct sockaddr_un {
    sa_family_t sun_family;               /* AF_UNIX */
    char        sun_path[108];            /* Pathname */
};

struct iovec {
    void* base;
    size_t len;
};

struct msghdr {
    void* name;
    socklen_t namelen;
    struct iovec* iov;
    size_t iovlen;
    void* control;
    size_t controllen;
    int flags;
};

struct poll {
    int fd;
    short events;
    short returnedEvents;
};

class Socket;

struct SocketConnection {
    Socket* socket;
    int connectionType;
};

enum {
    StreamSocket = 4,
    DatagramSocket = 1,
    SequencedSocket = 3,
    RawSocket = 0x10000,
    ReliableDatagramSocket = 0x20000,
    // Packet is considered obsolete
};

enum {
    InternetProtcol = 1,
    InternetProtcol6 = 2,
    UnixDomain = 3,
    LocalDomain = 3,
};

enum {
    ClientRole,
    ServerRole,
};

class Socket : public FsNode {
protected:
    int type = 0; // Type (Stream, Datagram, etc.)
    int domain = 0; // Domain (UNIX, INET, etc.)
    semaphore_t pendingConnections; // Pending Connections semaphore
    List<Socket*> pending; // Pending Connections

    bool bound = false; // Has it been bound to an address>
    bool passive = false; // Listening?
    bool blocking = true;
public:
    bool connected = false; // Connected?
    int role; // Server/Client
    
    static Socket* CreateSocket(int domain, int type, int protocol);

    Socket(int type, int protocol);
    virtual ~Socket();
    
    virtual int ConnectTo(Socket* client);

    virtual Socket* Accept(sockaddr* addr, socklen_t* addrlen);
    virtual Socket* Accept(sockaddr* addr, socklen_t* addrlen, int mode);
    virtual int Bind(const sockaddr* addr, socklen_t addrlen);
    virtual int Connect(const sockaddr* addr, socklen_t addrlen);
    virtual int Listen(int backlog);
    
    virtual int64_t ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen);
    virtual int64_t Receive(void* buffer, size_t len, int flags);
    virtual ssize_t Read(size_t offset, size_t size, uint8_t* buffer);
    
    virtual int64_t SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen);
    virtual int64_t Send(void* buffer, size_t len, int flags);
    virtual ssize_t Write(size_t offset, size_t size, uint8_t* buffer);
    
    virtual fs_fd_t* Open(size_t flags);
    virtual void Close();

    virtual int GetDomain() { return domain; }
    virtual int IsListening() { return passive; }
    virtual int IsBlocking() { return blocking; }
    virtual int IsConnected() { return connected; }
};

class LocalSocket : public Socket {
public:
    LocalSocket* peer = nullptr;

    Stream* inbound = nullptr;
    Stream* outbound = nullptr;

    LocalSocket(int type, int protocol);

    int ConnectTo(Socket* client);
    
    Socket* Accept(sockaddr* addr, socklen_t* addrlen, int mode);
    int Bind(const sockaddr* addr, socklen_t addrlen);
    int Connect(const sockaddr* addr, socklen_t addrlen);
    int Listen(int backlog);
    
    fs_fd_t* Open(size_t flags);
    void Close();
    
    int64_t ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen);
    int64_t SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen);

    bool CanRead() { if(inbound) return !inbound->Empty(); else return false; }
};

namespace SocketManager{
    typedef struct SocketBinding{
        char* address;
        Socket* socket;
    } sock_binding_t;

    Socket* ResolveSocketAddress(const sockaddr* addr, socklen_t addrlen);
    int BindSocket(Socket* sock, const sockaddr* addr, socklen_t addrlen);
    void DestroySocket(Socket* sock);
}