#include <net/net.h>
#include <net/socket.h>

#include <errno.h>

namespace Network {
    namespace TCP {
        HashMap<uint16_t, TCPSocket*> sockets;
        uint16_t nextEphemeralPort = EPHEMERAL_PORT_RANGE_START;

        Socket* FindSocket(BigEndian<uint16_t> port){
            return sockets.get(port.value);
        }

        int AcquirePort(TCPSocket* sock, unsigned int port){
            if(!port || port > PORT_MAX){
                Log::Warning("[Network] AcquirePort: Invalid port: %d", port);
                return -EINVAL;
            }

            if(sockets.get(port)){
                Log::Warning("[Network] AcquirePort: Port %d in use!", port);
                return -EADDRINUSE;
            }

            sockets.insert(port, sock);
            
            return 0;
        }

        unsigned short AllocatePort(TCPSocket* sock){
            unsigned short port = EPHEMERAL_PORT_RANGE_START;

            if(nextEphemeralPort < PORT_MAX){
                port = nextEphemeralPort++;
                
                if(AcquirePort(sock, port)){
                    port = 0;
                }
            } else {
                while(port < EPHEMERAL_PORT_RANGE_END && AcquirePort(sock, port)) port++;
            }

            if(port > EPHEMERAL_PORT_RANGE_END || port <= 0){
                Log::Warning("[Network] Could not allocate ephemeral port!");
                return 0;
            }

            return port;
        }

        int ReleasePort(unsigned short port){
            assert(port <= PORT_MAX);

            sockets.remove(port);

            return 0;
        }

        int SendTCP(void* data, size_t length, IPv4Address& destination, MACAddress& immediateDestination, TCPHeader& header, NetworkAdapter* adapter = nullptr){
            return 0;
        }

        void OnReceiveTCP(IPv4Header& ipHeader, void* data, size_t length){

        }

        void TCPSocket::Synchronize(){ // TCP SYN (Establish a connection to the server)

        }
        
        void TCPSocket::Acknowledge(){ // TCP ACK (Acknowledge connection)

        }
        
        void TCPSocket::SynchronizeAcknowledge(){ // TCP SYN-ACK (Establish connection to client and acknowledge the connection

        }

        unsigned short TCPSocket::AllocatePort(){
            return Network::TCP::AllocatePort(this);
        }

        int TCPSocket::AcquirePort(uint16_t port){
            return Network::TCP::AcquirePort(this, port);
        }

        int TCPSocket::ReleasePort(uint16_t port){
            return Network::TCP::ReleasePort(port);
        }

        TCPSocket::TCPSocket(int type, int protocol) : IPSocket(type, protocol) {
            assert(type == StreamSocket);
        }

        TCPSocket::~TCPSocket(){

        }

        Socket* TCPSocket::Accept(sockaddr* addr, socklen_t* addrlen, int mode){
            return nullptr;
        }

        int TCPSocket::Bind(const sockaddr* addr, socklen_t addrlen){
            if(int e = IPSocket::Bind(addr, addrlen)){
                return e;
            }

            assert(port && port < PORT_MAX);

            bound = true;

            return 0;
        }

        int TCPSocket::Connect(const sockaddr* addr, socklen_t addrlen){
            return -ENOSYS;   
        }

        int TCPSocket::Listen(int backlog){
            if(!bound){
                return -EDESTADDRREQ;
            }

            if(connected){
                return -EINVAL;
            }

            passive = true;

            if(backlog > 0){
                pendingConnections = Semaphore(backlog - pending.get_length());
            }

            return 0;
        }

    }
}