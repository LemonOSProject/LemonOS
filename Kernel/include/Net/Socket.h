#pragma once

#include <Fs/Filesystem.h>

#include <Net/Net.h>
#include <Net/If.h>

#include <stdint.h>
#include <stddef.h>
#include <Lock.h>
#include <Stream.h>
#include <List.h>

#include <abi-bits/in.h>
#include <abi-bits/socket.h>
#include <bits/posix/socklen_t.h>

#define CONNECTION_BACKLOG 128

#define STREAM_MAX_BUFSIZE 0x20000 // 128 KB

struct rtentry {
	unsigned long rt_pad1;
	struct sockaddr rt_dst;
	struct sockaddr rt_gateway;
	struct sockaddr rt_genmask;
	unsigned short rt_flags;
	short rt_pad2;
	unsigned long rt_pad3;
	void *rt_pad4;
	short rt_metric;
	char *rt_dev;
	unsigned long rt_mtu;
	unsigned long rt_window;
	unsigned short rt_irtt;
};

#define RTF_UP 0x0001 // Route not deleted
#define RTF_GATEWAY 0x0002 // Route points not to the ultimate destination but to an immediate destination
#define RTF_HOST 0x0004
#define RTF_REINSTATE 0x0008
#define RTF_DYNAMIC 0x0010
#define RTF_MODIFIED 0x0020
#define RTF_MTU 0x0040
#define RTF_MSS RTF_MTU
#define RTF_WINDOW 0x0080
#define RTF_IRTT 0x0100
#define RTF_REJECT 0x0200

struct sockaddr_un {
    sa_family_t sun_family;               /* AF_UNIX */
    char        sun_path[108];            /* Pathname */
};

struct cmsghdr {
	socklen_t cmsg_len;
	int cmsg_level;
	int cmsg_type;
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
    StreamSocket = SOCK_STREAM,
    DatagramSocket = SOCK_DGRAM,
    SequencedSocket = SOCK_SEQPACKET,
    RawSocket = SOCK_RAW,
};

enum SocketProtocol {
    UnixDomain = PF_UNIX,
    LocalDomain = PF_LOCAL,
    InternetProtocol = PF_INET,
    InternetProtocol6 = PF_INET6,
};

class Socket : public FsNode {
protected:
    int type = 0; // Type (Stream, Datagram, etc.)
    int domain = 0; // Domain (UNIX, INET, etc.)
    Semaphore pendingConnections = Semaphore(CONNECTION_BACKLOG); // Pending Connections semaphore
    List<Socket*> pending; // Pending Connections

    bool bound = false; // Has it been bound to an address>
    bool passive = false; // Listening?
    bool blocking = true;
public: 
    bool connected = false; // Connected?
    
    static int CreateSocket(int domain, int type, int protocol, Socket** sock);

    Socket(int type, int protocol);
    virtual ~Socket();

    virtual Socket* Accept(sockaddr* addr, socklen_t* addrlen);
    virtual Socket* Accept(sockaddr* addr, socklen_t* addrlen, int mode);
    virtual int Bind(const sockaddr* addr, socklen_t addrlen);
    virtual int Connect(const sockaddr* addr, socklen_t addrlen);
    virtual int Listen(int backlog);
    
    virtual int64_t ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);
    virtual int64_t Receive(void* buffer, size_t len, int flags);
    virtual ssize_t Read(size_t offset, size_t size, uint8_t* buffer);
    
    virtual int64_t SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);
    virtual int64_t Send(void* buffer, size_t len, int flags);
    virtual ssize_t Write(size_t offset, size_t size, uint8_t* buffer);

    virtual int SetSocketOptions(int level, int opt, const void* optValue, socklen_t optLength);
    virtual int GetSocketOptions(int level, int opt, void* optValue, socklen_t* optLength);
    
    virtual fs_fd_t* Open(size_t flags);
    virtual void Close();

    virtual void Watch(FilesystemWatcher& watcher, int events);
    virtual void Unwatch(FilesystemWatcher& watcher);

    virtual int GetDomain() { return domain; }
    virtual int IsListening() { return passive; }
    virtual int IsBlocking() { return blocking; }
    virtual int IsConnected() { return connected; }

    virtual int PendingConnections() { return pending.get_length(); }
};

class LocalSocket : public Socket {
    lock_t slock = 0;

    lock_t watcherLock = 0;
    List<FilesystemWatcher*> watching;
public:
    LocalSocket* peer = nullptr;

    Stream* inbound = nullptr;
    Stream* outbound = nullptr;

    LocalSocket(int type, int protocol);
    ~LocalSocket();

    static LocalSocket* CreatePairedSocket(LocalSocket* client);

    int ConnectTo(Socket* client);
    void DisconnectPeer();

    void OnDisconnect();
    
    Socket* Accept(sockaddr* addr, socklen_t* addrlen, int mode);
    int Bind(const sockaddr* addr, socklen_t addrlen);
    int Connect(const sockaddr* addr, socklen_t addrlen);
    int Listen(int backlog);
    
    fs_fd_t* Open(size_t flags);
    void Close();
    
    int64_t ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);
    int64_t SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);
    
    void Watch(FilesystemWatcher& watcher, int events);
    void Unwatch(FilesystemWatcher& watcher);

    bool CanRead() { if(inbound) return !inbound->Empty(); else return false; }
};

class IPSocket
    : public Socket {
public:
    IPSocket(int type, int protocol);
    virtual ~IPSocket();

    int Ioctl(uint64_t cmd, uint64_t arg);
    
    Socket* Accept(sockaddr* addr, socklen_t* addrlen, int mode);
    int Bind(const sockaddr* addr, socklen_t addrlen);
    int Connect(const sockaddr* addr, socklen_t addrlen);
    int Listen(int backlog);

    int SetSocketOptions(int level, int opt, const void* optValue, socklen_t optLength);
    int GetSocketOptions(int level, int opt, void* optValue, socklen_t* optLength);
    
    inline IPv4Address LocalIPAddress() { return address; }
    inline IPv4Address PeerIPAddress() { return peerAddress; }
    inline uint16_t LocalPort() { return port; }
    inline uint16_t PeerPort() { return destinationPort; }

    void Close();
    
    virtual int64_t ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);
    virtual int64_t SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);

    Network::NetworkAdapter* adapter = nullptr; // Bound adapter
protected:
    IPv4Address address = 0;
    IPv4Address peerAddress = 0;
    BigEndian<uint16_t> port = 0;
    BigEndian<uint16_t> destinationPort = 0;

    bool pktInfo = false; // Check for packet info field?

    List<NetworkPacket> pQueue;

    virtual unsigned short AllocatePort() = 0;
    virtual int AcquirePort(uint16_t port) = 0;
    virtual int ReleasePort() = 0;
    int64_t OnReceive(void* buffer, size_t len);
};

namespace Network::UDP{
    class UDPSocket final
        : public IPSocket {
    protected:
        friend void OnReceiveUDP(IPv4Header& ipHeader, void* data, size_t length);

        struct UDPPacket{
            IPv4Address sourceIP;
            BigEndian<uint16_t> sourcePort;
            size_t length;
            uint8_t* data;
        };

        lock_t packetsLock = 0;
        List<UDPPacket> packets;

        unsigned short AllocatePort();
        int AcquirePort(uint16_t port);
        int ReleasePort();

        int64_t OnReceive(IPv4Address& sourceIP, BigEndian<uint16_t> sourcePort, void* buffer, size_t len);
    public:
        UDPSocket(int type, int protocol);
        ~UDPSocket();

        Socket* Accept(sockaddr* addr, socklen_t* addrlen, int mode);
        int Bind(const sockaddr* addr, socklen_t addrlen);
        int Connect(const sockaddr* addr, socklen_t addrlen);
        int Listen(int backlog);

        int64_t ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);
        int64_t SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);
    };
}

namespace Network::TCP {
    class TCPSocket final
        : public IPSocket {
    public:
        TCPSocket(int type, int protocol);
        ~TCPSocket();

        Socket* Accept(sockaddr* addr, socklen_t* addrlen, int mode);
        int Bind(const sockaddr* addr, socklen_t addrlen);
        int Connect(const sockaddr* addr, socklen_t addrlen);
        int Listen(int backlog);

        int64_t ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);
        int64_t SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen, const void* ancillary = nullptr, size_t ancillaryLen = 0);

        int SetSocketOptions(int level, int opt, const void* optValue, socklen_t optLength);
        int GetSocketOptions(int level, int opt, void* optValue, socklen_t* optLength);
        
        void Close();
        
    protected:
        bool m_fileClosed = false;
        bool m_noDelay = false; // Disable 'Nagle's algorithm', we don't actualy implement this yet 
        bool m_keepAlive = false; // We haven't implmented this yet

        friend void OnReceiveTCP(IPv4Header& ipHeader, void* data, size_t length);

        void OnReceive(const IPv4Address& source, const IPv4Address& dest, uint8_t* data, size_t length);

        int Synchronize(uint32_t seqNumber); // TCP SYN (Establish a connection to the server)
        int Acknowledge(uint32_t ackNumber); // TCP ACK (Acknowledge connection)
        int SynchronizeAcknowledge(uint32_t seqNumber, uint32_t ackNumber); // TCP SYN-ACK (Establish connection to client and acknowledge the connection)
        int Finish(); // TCP FIN (Last packet from sender)
        int FinishAcknowledge(uint32_t ackNumber); // TCP FIN-ACK (Last packet from sender, acknowledge)
        int Reset(); // TCP RST (Abort connection)

        unsigned short AllocatePort();
        int AcquirePort(uint16_t port);
        int ReleasePort();

        struct TCPPacket {
            TCPHeader header;
            uint32_t sequenceNumber;
            uint32_t length;
            uint8_t* data;
        };

        // As per RFC 793
        enum State {
            TCPStateUnknown,
            TCPStateListen, // Awaiting incoming connections/SYN packets
            TCPStateSyn, // SYN has been sent, awaitng a SYN-ACK response
            TCPStateSynAck, // SYN-ACK has been sent, awaiting an ACK response
            TCPStateEstablished, // Connection has been established
            TCPStateFinWait1, // Waiting for an ACK or FIN-ACK after our FIN
            TCPStateFinWait2, // Waiting for the peer to send FIN
            TCPStateCloseWait, // Waiting for the last process to close the socket
            TCPStateLastAck, // Waiting for a final ACK after our FIN 
            TCPStateTimeWait, // Waiting to ensure that the peer recieved its ACK
        };

        State state = TCPStateUnknown;
        uint32_t m_sequenceNumber;
        uint32_t m_lastAcknowledged; // Last acknowledged sequence number

        uint32_t m_remoteSequenceNumber; // Sequence number of the remote endpoint

        lock_t m_unacknowledgedPacketsLock = 0;
        List<TCPPacket> m_unacknowledgedPackets; // Unacknowledged outbound packets
        DataStream m_inboundData = DataStream(512);
    };
}

namespace SocketManager{
    typedef struct SocketBinding{
        char* address;
        Socket* socket;
    } sock_binding_t;

    Socket* ResolveSocketAddress(const sockaddr* addr, socklen_t addrlen);
    int BindSocket(Socket* sock, const sockaddr* addr, socklen_t addrlen);
    void DestroySocket(Socket* sock);
}