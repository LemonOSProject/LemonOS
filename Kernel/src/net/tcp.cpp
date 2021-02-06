#include <net/net.h>
#include <net/socket.h>
#include <net/networkadapter.h>

#include <errno.h>

namespace Network {
    namespace TCP {
        HashMap<uint16_t, TCPSocket*> sockets;
        uint16_t nextEphemeralPort = EPHEMERAL_PORT_RANGE_START;

        Socket* FindSocket(BigEndian<uint16_t> port){
            TCPSocket* sock = nullptr;
            
            if(!sockets.get(port.value, sock)){
                return nullptr;
            }

            return sock;
        }

        int AcquirePort(TCPSocket* sock, unsigned int port){
            if(!port || port > PORT_MAX){
                Log::Warning("[Network] AcquirePort: Invalid port: %d", port);
                return -EINVAL;
            }

            if(sockets.find(port)){
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

        BigEndian<uint16_t> CalculateTCPChecksum(const IPv4Address& src, const IPv4Address& dest, void* data, uint16_t size){
            struct PseudoIPv4Header{
                uint32_t source;
                uint32_t destination;
                uint8_t zero = 0;
                uint8_t protocol = IPv4ProtocolTCP;
                BigEndian<uint16_t> length = 0;
            } __attribute__((packed));
            uint32_t checksum = 0;

            PseudoIPv4Header pseudoHeader;
            pseudoHeader.source = src.value;
            pseudoHeader.destination = dest.value;
            pseudoHeader.zero = 0;
            pseudoHeader.protocol = IPv4ProtocolTCP;
            pseudoHeader.length = size;

            uint16_t* ptr = (uint16_t*)&pseudoHeader;
            uint16_t count = sizeof(PseudoIPv4Header);
            while(count >= 2){
                checksum += *ptr++;
                count -= 2;

                checksum = (checksum & 0xFFFF) + (checksum >> 16);
            }

            ptr = (uint16_t*)data;
            count = size;
            while(count >= 2){
                checksum += *ptr++;
                count -= 2;

                checksum = (checksum & 0xFFFF) + (checksum >> 16);
            }

            BigEndian<uint16_t> ret;
            ret.value = ~checksum;
            return ret;
        }

        int SendTCP(void* data, size_t length, IPv4Address& destination, TCPHeader& header, NetworkAdapter* adapter = nullptr){
            return 0;
        }

        void OnReceiveTCP(IPv4Header& ipHeader, void* data, size_t length){
            TCPHeader* tcpHeader = reinterpret_cast<TCPHeader*>(data);
            BigEndian<uint16_t> checksum = tcpHeader->checksum;

            tcpHeader->checksum = 0;
            if(CalculateTCPChecksum(ipHeader.sourceIP, ipHeader.destIP, data, length) != checksum){
                Log::Warning("[Network] [TCP] Dropping Packet (invalid checksum)");
            }
            tcpHeader->checksum = checksum;

            
        }

        int TCPSocket::Synchronize(){ // TCP SYN (Establish a connection to the server)
            TCPHeader tcpHeader;
            memset(&tcpHeader, 0, sizeof(TCPHeader));
            
            tcpHeader.srcPort = port;
            tcpHeader.destPort = destinationPort;
            tcpHeader.sequence = 0;
            tcpHeader.acknowledgementNumber = 0;

            tcpHeader.dataOffset = sizeof(TCPHeader) / 4; // Size of the TCP Header in DWORDs
            tcpHeader.syn = 1; // SYN/Synchronize

            tcpHeader.windowSize = 65535;

            tcpHeader.checksum = CalculateTCPChecksum(adapter->adapterIP, peerAddress, &tcpHeader, sizeof(TCPHeader));

            if(int e = SendIPv4(&tcpHeader, sizeof(TCPHeader), address, peerAddress, IPv4ProtocolTCP, adapter); e){
                return e;
            }

            return 0;
        }
        
        int TCPSocket::Acknowledge(uint32_t ackNumber){ // TCP ACK (Acknowledge connection)
            TCPHeader tcpHeader;
            memset(&tcpHeader, 0, sizeof(TCPHeader));
            
            tcpHeader.srcPort = port;
            tcpHeader.destPort = destinationPort;
            tcpHeader.sequence = 0;
            tcpHeader.acknowledgementNumber = ackNumber;

            tcpHeader.dataOffset = sizeof(TCPHeader) / 4; // Size of the TCP Header in DWORDs
            tcpHeader.ack = 1; // ACK/Acknowledge

            tcpHeader.windowSize = 65535;

            tcpHeader.checksum = CalculateTCPChecksum(adapter->adapterIP, peerAddress, &tcpHeader, sizeof(TCPHeader));

            if(int e = SendIPv4(&tcpHeader, sizeof(TCPHeader), address, peerAddress, IPv4ProtocolTCP, adapter); e){
                return e;
            }

            return 0;
        }
        
        int TCPSocket::SynchronizeAcknowledge(uint32_t ackNumber){ // TCP SYN-ACK (Establish connection to client and acknowledge the connection
            TCPHeader tcpHeader;
            memset(&tcpHeader, 0, sizeof(TCPHeader));
            
            tcpHeader.srcPort = port;
            tcpHeader.destPort = destinationPort;
            tcpHeader.sequence = 0;
            tcpHeader.acknowledgementNumber = ackNumber;

            tcpHeader.dataOffset = sizeof(TCPHeader) / 4; // Size of the TCP Header in DWORDs
            tcpHeader.syn = 1; // SYN/Synchronize
            tcpHeader.ack = 1; // ACK/Acknowledge

            tcpHeader.windowSize = 65535;

            tcpHeader.checksum = CalculateTCPChecksum(adapter->adapterIP, peerAddress, &tcpHeader, sizeof(TCPHeader));

            if(int e = SendIPv4(&tcpHeader, sizeof(TCPHeader), address, peerAddress, IPv4ProtocolTCP, adapter); e){
                return e;
            }

            return 0;
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
            if(addrlen < sizeof(sockaddr_in)){
                return -EINVAL; // Invalid length
            } else if(addr->family != AF_INET){
                return -EAFNOSUPPORT; // Not AF_INET
            }

            const sockaddr_in* inAddr = reinterpret_cast<const sockaddr_in*>(addr);
            peerAddress.value = inAddr->sin_addr.s_addr;
            destinationPort.value = inAddr->sin_port;

            if(!port){
                port = AllocatePort(); // Allocate ephemeral port

                if(!port){
                    Log::Warning("[TCPSocket] Failed to allocate temporary port!");
                    return -EADDRNOTAVAIL;
                }
            }

            if(inAddr->sin_addr.s_addr == INADDR_BROADCAST){
                return -ECONNREFUSED;
            }

            long retryPeriod = TCP_RETRY_MIN;
            while(state != TCPStateEstablished){
                FilesystemBlocker bl(this);

                long timeout = retryPeriod;
                if(Scheduler::GetCurrentThread()->Block(&bl, retryPeriod)){
                    return -EINTR;
                }

                if(timeout <= 0){
                    retryPeriod *= retryPeriod;
                }
            }

            
            return 0;
        }

        int TCPSocket::Listen(int backlog){
            if(!bound){
                return -EDESTADDRREQ;
            }

            if(connected){
                return -EISCONN;
            }

            state = TCPStateListen;

            if(backlog > 0){
                pendingConnections = Semaphore(backlog - pending.get_length());
            }

            return 0;
        }

    }
}