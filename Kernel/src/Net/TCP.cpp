#include <Net/Net.h>
#include <Net/Socket.h>
#include <Net/Adapter.h>

#include <Timer.h>
#include <Math.h>

#include <Errno.h>

// Create an identifier for a connection with the source IP, dest IP, source port and destination port.
// This allows for one connection per port per remote address per local adddress
struct TCPConnectionIdentifier{
    IPv4Address localIP; // Local TCP Endpoint IP
    IPv4Address remoteIP; // Remote TCP Endpoint IP
    uint16_t localPort; // Local Port
    uint16_t remotePort; // Remote Port

    TCPConnectionIdentifier() = default;

    TCPConnectionIdentifier(const IPv4Address& local, const IPv4Address& remote, uint16_t lPort, uint16_t rPort) : localIP(local), remoteIP(remote), localPort(lPort), remotePort(rPort){

    }

    inline bool operator==(const TCPConnectionIdentifier& other) const{
        return remoteIP.value == other.remoteIP.value && localIP.value == other.localIP.value && remotePort == other.remotePort && localPort == other.localPort;
    }
};

template<>
inline unsigned Hash<TCPConnectionIdentifier>(const TCPConnectionIdentifier& id){
    return ::Hash(id.remoteIP.value) ^ ::Hash(id.localIP.value) ^ ::Hash(id.remotePort) ^ ::Hash(id.localPort);
}

namespace Network {
    namespace TCP {
        List<TCPSocket*> closedSockets;
        HashMap<TCPConnectionIdentifier, TCPSocket*> sockets;
        uint16_t nextEphemeralPort = EPHEMERAL_PORT_RANGE_START;

        TCPSocket* FindSocket(TCPConnectionIdentifier id){
            TCPSocket* sock = nullptr;
            
            if(sockets.get(id, sock)){
                return sock;
            }

            id.remoteIP = INADDR_ANY;
            id.remotePort = INADDR_ANY;
            
            if(sockets.get(id, sock)){
                return sock; // We may want to initiate a connection to a listen socket
            }

            id.localIP.value = INADDR_ANY;
            
            if(sockets.get(id, sock)){
                return sock;
            }

            return nullptr;
        }

        int AcquirePort(TCPSocket* sock, const IPv4Address& localAddress, const IPv4Address& remoteAddress, uint16_t port, uint16_t remotePort){
            if(!port || port > PORT_MAX){
                Log::Warning("[Network] AcquirePort: Invalid port: %d", port);
                return -EINVAL;
            }

            TCPConnectionIdentifier id = TCPConnectionIdentifier(localAddress, remoteAddress, port, remotePort);

            if(sockets.find(id)){
                Log::Warning("[Network] AcquirePort: Port %d in use on %d.%d.%d.%d!", port, localAddress.data[0], localAddress.data[0], localAddress.data[1], localAddress.data[2], localAddress.data[3]);
                return -EADDRINUSE;
            }

            sockets.insert(id, sock);
            
            return 0;
        }

        unsigned short AllocatePort(TCPSocket* sock){
            unsigned short port = EPHEMERAL_PORT_RANGE_START;

            if(nextEphemeralPort < PORT_MAX){
                port = nextEphemeralPort++;
                
                if(AcquirePort(sock, sock->LocalIPAddress(), sock->PeerIPAddress(), port, sock->PeerPort())){
                    port = 0;
                }
            } else {
                while(port < EPHEMERAL_PORT_RANGE_END && AcquirePort(sock, sock->LocalIPAddress(), sock->PeerIPAddress(), port, sock->PeerPort())) port++;
            }

            if(port > EPHEMERAL_PORT_RANGE_END || port <= 0){
                Log::Warning("[Network] Could not allocate ephemeral port!");
                return 0;
            }

            return port;
        }

        int ReleasePort(TCPSocket* sock){
            sockets.removeValue(sock);

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
            }

            ptr = (uint16_t*)data;
            count = size;
            while(count >= 2){
                checksum += *ptr++;
                count -= 2;
            }

            if(count){ // Uneven amount of data
                checksum += ((uint16_t)(*reinterpret_cast<uint8_t*>(ptr)));
            }

            checksum = (checksum & 0xFFFF) + (checksum >> 16);

            BigEndian<uint16_t> ret;
            ret.value = ~checksum;
            return ret;
        }

        int SendTCP(void* data, size_t length, IPv4Address& source, IPv4Address& destination, TCPHeader& header, NetworkAdapter* adapter = nullptr){
            uint8_t buffer[1600];

            if(length + sizeof(TCPHeader) > 1600){
                length = 1600 - sizeof(TCPHeader);
            }

            TCPHeader* tcpHeader = reinterpret_cast<TCPHeader*>(buffer);

            memcpy(buffer + sizeof(TCPHeader), data, length);

            *tcpHeader = header;

            tcpHeader->checksum = 0;
            tcpHeader->checksum = CalculateTCPChecksum(source, destination, buffer, length + sizeof(TCPHeader));

            if(int e = SendIPv4(buffer, length + sizeof(TCPHeader), source, destination, IPv4ProtocolTCP, adapter); e){
                return e;
            }

            return length;
        }

        void OnReceiveTCP(IPv4Header& ipHeader, void* data, size_t length){
            TCPHeader* tcpHeader = reinterpret_cast<TCPHeader*>(data);
            BigEndian<uint16_t> checksum = tcpHeader->checksum;

            Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] Recieving Packet from %hd.%hd.%hd.%hd:%hu (dest: %hd.%hd.%hd.%hd:%hu)!", ipHeader.sourceIP.data[0], ipHeader.sourceIP.data[1], ipHeader.sourceIP.data[2], ipHeader.sourceIP.data[3], (uint16_t)tcpHeader->srcPort, ipHeader.destIP.data[0], ipHeader.destIP.data[1], ipHeader.destIP.data[2], ipHeader.destIP.data[3], (uint16_t)tcpHeader->destPort);

            /*tcpHeader->checksum = 0;
            if(uint16_t chk = CalculateTCPChecksum(ipHeader.sourceIP, ipHeader.destIP, data, ipHeader.length).value; chk != checksum.value){
                Log::Warning("[Network] [TCP] Dropping Packet (invalid checksum %hx, should be: %hx)", (uint16_t)checksum, EndianBigToLittle16(chk));
            }*/
            tcpHeader->checksum = checksum;

            if(tcpHeader->dataOffset * 4 < sizeof(TCPHeader)){
                return; // Invalid data offset (must be at least 5)
            }

            TCPSocket* sock = FindSocket(TCPConnectionIdentifier(ipHeader.destIP, ipHeader.sourceIP, tcpHeader->destPort, tcpHeader->srcPort));
            if(!sock){
                Log::Info("no such sock! (Source: %hd.%hd.%hd.%hd:%hu)", ipHeader.sourceIP.data[0], ipHeader.sourceIP.data[1], ipHeader.sourceIP.data[2], ipHeader.sourceIP.data[3], tcpHeader->srcPort);
                return; // Port not bound to socket
            }

            if(sock->state == TCPSocket::TCPStateUnknown){
                return; // Has not attempted to open connecction and is not a listen socket
            }

            sock->OnReceive(ipHeader.sourceIP, ipHeader.destIP, reinterpret_cast<uint8_t*>(data), length);
        }

        void TCPSocket::OnReceive(const IPv4Address& source, const IPv4Address& dest, uint8_t* data, size_t length){
            TCPHeader* tcpHeader = reinterpret_cast<TCPHeader*>(data); // Checksum has already been verified

            if(state == TCPStateUnknown){
                return; // We should not be receiving packets as we have not opened a connection and we are not listening
            }

            if(state == TCPStateListen){
                bool syn = tcpHeader->syn;

                // Ignore ECE flag as when SYN is set ECE indicates whether the peer is ECE *capable*
                uint16_t other = (tcpHeader->flags & (TCPHeader::FlagsMask ^ (TCPHeader::SYN | TCPHeader::ECE))); // Get all other flags

                if(other){
                    Log::Debug(debugLevelNetwork, DebugLevelNormal, "[Network] [TCP] (State: LISTEN) Unexpected flags: %hx", other);
                    return; // Unexpected flags
                }

                if(syn){
                    Log::Warning("[Network] [TCP] (State: LISTEN) TCP listen sockets not yet supported!");
                }
                return;
            }

            if(tcpHeader->rst){
                state = TCPStateUnknown; // Abort connection

                UnblockAll();
            } else if(state == TCPStateSyn){
                bool ack = tcpHeader->ack;
                bool syn = tcpHeader->syn;

                // Ignore ECE flag as when SYN is set ECE indicates whether the peer is ECE *capable*
                uint16_t other = (tcpHeader->flags & (TCPHeader::FlagsMask ^ (TCPHeader::ACK | TCPHeader::SYN | TCPHeader::ECE))); // Get all other flags

                if(other){
                    Log::Debug(debugLevelNetwork, DebugLevelNormal, "[Network] [TCP] (State: SYN-SENT) Unexpected flags: %hx", other);
                    return; // Unexpected flags
                }

                if(ack && syn){ // It is important that we recieve a SYN and ACK
                    Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] (State: SYN-SENT) Recieved SYN-ACK (Sequence number: %u) from %d.%d.%d.%d:%d", tcpHeader->sequence, source.data[0], source.data[1], source.data[2], source.data[3], (uint16_t)tcpHeader->srcPort);

                    m_remoteSequenceNumber = tcpHeader->sequence;
                    m_lastAcknowledged = tcpHeader->acknowledgementNumber;

                    if(m_lastAcknowledged == m_sequenceNumber){ // The ACK number must be equal to the sequence number.
                        state = TCPStateEstablished; // Our SYN has been acknowledged with a SYN-ACK

                        UnblockAll(); // Unblock waiting threads
                    }
                }

                return;
            } else if(state == TCPStateSynAck){
                bool ack = tcpHeader->ack;

                if(ack){
                    Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] (State: SYN-RECV) Recieved ACK from %d.%d.%d.%d:%d", source.data[0], source.data[1], source.data[2], source.data[3], (uint16_t)tcpHeader->srcPort);

                    m_remoteSequenceNumber = tcpHeader->sequence;

                    state = TCPStateEstablished; // Our SYN has been acknowledged with a SYN-ACK

                    UnblockAll(); // Unblock waiting threads
                }
            } else if(state == TCPStateEstablished){
                bool ack = tcpHeader->ack;
                bool psh = tcpHeader->psh;
                bool fin = tcpHeader->fin;
                
                bool doUnblock = false;
                uint16_t other = (tcpHeader->flags & (TCPHeader::FlagsMask ^ (TCPHeader::ACK | TCPHeader::PSH | TCPHeader::FIN | TCPHeader::ECE))); // Get all other flags

                if(other){
                    return; // Unsupported flags
                }

                if(tcpHeader->sequence != m_remoteSequenceNumber){
                    Log::Debug(debugLevelNetwork, DebugLevelNormal, "[Network] [TCP] (State: ESTABLISHED) Received packet with sequence number of %d, Expected %d", tcpHeader->sequence, m_remoteSequenceNumber);
                    return; // Either a previous packet has been dropped, or it is retransmitting a previous packet
                }

                uint16_t dataOffset = tcpHeader->dataOffset * 4;
                uint16_t dataLength = length - dataOffset;

                if(ack){
                    if(tcpHeader->acknowledgementNumber > m_sequenceNumber){
                        Log::Debug(debugLevelNetwork, DebugLevelNormal, "[Network] [TCP] (State: ESTABLIHSED) recieved ACK wth ack number > sequence number");
                        Acknowledge(m_remoteSequenceNumber);
                        return;
                    }

                    acquireLock(&m_unacknowledgedPacketsLock);

                    m_lastAcknowledged = tcpHeader->acknowledgementNumber;

                    for(auto it = m_unacknowledgedPackets.begin(); it != m_unacknowledgedPackets.end(); it++){
                        if(it->sequenceNumber <= m_lastAcknowledged){
                            m_unacknowledgedPackets.remove(it);
                        }

                        if(it == m_unacknowledgedPackets.end()){
                            break;
                        }
                    }

                    releaseLock(&m_unacknowledgedPacketsLock);
                }

                Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] Recieving %d bytes of data (Flags: %hx, total len: %d)", dataLength, tcpHeader->flags & TCPHeader::FlagsMask);

                if(dataLength){
                    m_inboundData.Write(data + dataOffset, dataLength);

                    acquireLock(&blockedLock);
                    FilesystemBlocker* bl = blocked.get_front();
                    while(bl){
                        FilesystemBlocker* next = blocked.next(bl);

                        if(bl->RequestedLength() <= m_inboundData.Pos()){
                            bl->Unblock();
                        }

                        bl = next;
                    }
                    releaseLock(&blockedLock);

                    m_remoteSequenceNumber = tcpHeader->sequence + dataLength;

                    Acknowledge(m_remoteSequenceNumber);
                }

                if(psh){
                    Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] PSH!");

                    doUnblock = true;
                }

                if(fin){
                    Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] (State: ESTABLISHED) Peer closed connection with FIN, entering CLOSE-WAIT");
                    state = TCPStateCloseWait; // Connection ended, wait for process(es) to close file descriptors

                    Acknowledge(m_remoteSequenceNumber);
                    doUnblock = true;
                }

                if(doUnblock){
                    UnblockAll();
                }
            } else if(state == TCPStateFinWait1){
                bool ack = tcpHeader->ack;
                bool fin = tcpHeader->fin;
                uint16_t other = (tcpHeader->flags & (TCPHeader::FlagsMask ^ (TCPHeader::ACK | TCPHeader::FIN))); // Get all other flags

                if(other){
                    Log::Debug(debugLevelNetwork, DebugLevelNormal, "[Network] [TCP] (State: FIN-WAIT-1) Unexpected flags: %hx", other);
                    return; // Unexpected flags
                }

                if(fin && ack){
                    state = TCPStateTimeWait;

                    Acknowledge(m_remoteSequenceNumber);
                } else if(ack){
                    m_remoteSequenceNumber = tcpHeader->sequence;

                    state = TCPStateFinWait2;
                } else if(fin){
                    m_remoteSequenceNumber = tcpHeader->sequence + 1;

                    FinishAcknowledge(m_remoteSequenceNumber);

                    state = TCPStateLastAck;
                }

                return;
            } else if(state == TCPStateFinWait2){
                bool fin = tcpHeader->fin;

                // Allow the remote endpoint to resend ACK
                uint16_t other = (tcpHeader->flags & (TCPHeader::FlagsMask ^ (TCPHeader::FIN | TCPHeader::ACK))); // Get all other flags

                if(other){
                    Log::Debug(debugLevelNetwork, DebugLevelNormal, "[Network] [TCP] (State: FIN-WAIT-2) Unexpected flags: %hx", other);
                    return; // Unexpected flags
                }

                if(fin){
                    m_remoteSequenceNumber = tcpHeader->sequence + 1;

                    state = TCPStateTimeWait;

                    Acknowledge(m_remoteSequenceNumber);
                }
            } else if(state == TCPStateCloseWait || state == TCPStateTimeWait){
                state = TCPStateUnknown;

                Reset(); // (CLOSE-WAIT) We should not be receiving packets on a close-wait / (TIME-WAIT) It did not receive our ACK, just reset
            } else if(state == TCPStateLastAck){
                bool ack = tcpHeader->ack;

                if(ack){
                    state = TCPStateUnknown; // We have closed successfully
                } else {
                    Log::Debug(debugLevelNetwork, DebugLevelNormal, "[Network] [TCP] (State: LAST_ACK) Unexpected flags: %hx. Expected ACK", tcpHeader->flags & TCPHeader::FlagsMask);

                    state = TCPStateUnknown;
                    Reset();
                }
            }

            if(m_fileClosed && state == TCPStateUnknown){
                closedSockets.remove(this);
                if(port) {
                    ReleasePort();
                }

                delete this;
            }
        }

        int TCPSocket::Synchronize(uint32_t seqNumber){ // TCP SYN (Establish a connection to the server)
            TCPHeader tcpHeader;
            memset(&tcpHeader, 0, sizeof(TCPHeader));
            
            Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] [SYN] Sequence Number: %u", seqNumber);

            tcpHeader.srcPort = port;
            tcpHeader.destPort = destinationPort;
            tcpHeader.sequence = seqNumber;
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

            Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] [ACK] Acknowledgement Number: %u", ackNumber);
            
            tcpHeader.srcPort = port;
            tcpHeader.destPort = destinationPort;
            tcpHeader.sequence = m_sequenceNumber;
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
        
        int TCPSocket::SynchronizeAcknowledge(uint32_t seqNumber, uint32_t ackNumber){ // TCP SYN-ACK (Establish connection to client and acknowledge the connection
            TCPHeader tcpHeader;
            memset(&tcpHeader, 0, sizeof(TCPHeader));

            Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] [SYN-ACK] Sequence Number: %u, Acknowledgement Number: %u", seqNumber, ackNumber);
            
            tcpHeader.srcPort = port;
            tcpHeader.destPort = destinationPort;
            tcpHeader.sequence = seqNumber;
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

        int TCPSocket::Finish(){ // TCP FIN (Last packet from sender)
            TCPHeader tcpHeader;
            memset(&tcpHeader, 0, sizeof(TCPHeader));

            Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] [FIN]");
            
            tcpHeader.srcPort = port;
            tcpHeader.destPort = destinationPort;
            tcpHeader.sequence = m_sequenceNumber;

            tcpHeader.dataOffset = sizeof(TCPHeader) / 4; // Size of the TCP Header in DWORDs
            tcpHeader.fin = 1; // FIN/Finish

            tcpHeader.windowSize = 65535;

            tcpHeader.checksum = CalculateTCPChecksum(adapter->adapterIP, peerAddress, &tcpHeader, sizeof(TCPHeader));

            if(int e = SendIPv4(&tcpHeader, sizeof(TCPHeader), address, peerAddress, IPv4ProtocolTCP, adapter); e){
                return e;
            }

            return 0;
        }

        int TCPSocket::FinishAcknowledge(uint32_t ackNumber){ // TCP FIN-ACK (Last packet from sender, acknowledge)
            TCPHeader tcpHeader;
            memset(&tcpHeader, 0, sizeof(TCPHeader));

            Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] [FIN-ACK] Acknowledgement Number: %u", ackNumber);
            
            tcpHeader.srcPort = port;
            tcpHeader.destPort = destinationPort;
            tcpHeader.sequence = m_sequenceNumber;
            tcpHeader.acknowledgementNumber = ackNumber;

            tcpHeader.dataOffset = sizeof(TCPHeader) / 4; // Size of the TCP Header in DWORDs
            tcpHeader.ack = 1; // ACK/Acknowledge
            tcpHeader.fin = 1;

            tcpHeader.windowSize = 65535;

            tcpHeader.checksum = CalculateTCPChecksum(adapter->adapterIP, peerAddress, &tcpHeader, sizeof(TCPHeader));

            if(int e = SendIPv4(&tcpHeader, sizeof(TCPHeader), address, peerAddress, IPv4ProtocolTCP, adapter); e){
                return e;
            }

            return 0;
        }

        int TCPSocket::Reset(){ // TCP RST (Abort connection)
            TCPHeader tcpHeader;
            memset(&tcpHeader, 0, sizeof(TCPHeader));

            Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] [RST]");
            
            tcpHeader.srcPort = port;
            tcpHeader.destPort = destinationPort;
            tcpHeader.sequence = m_sequenceNumber;

            tcpHeader.dataOffset = sizeof(TCPHeader) / 4; // Size of the TCP Header in DWORDs
            tcpHeader.rst = 1; // RST, Abort the connections

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
            return Network::TCP::AcquirePort(this, address, peerAddress, port, destinationPort);
        }

        int TCPSocket::ReleasePort(){
            return Network::TCP::ReleasePort(this);
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
            if(bound){
                return -EINVAL;
            }

            if(int e = IPSocket::Bind(addr, addrlen)){
                if(e == -2){
                    return -EADDRINUSE; // Failed to acquire port
                }
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

            if(IsConnected()){
                Log::Debug(debugLevelNetwork, DebugLevelNormal, "[TCP] Socket already connected!");
                return -EISCONN;
            }

            const sockaddr_in* inAddr = reinterpret_cast<const sockaddr_in*>(addr);
            peerAddress.value = inAddr->sin_addr.s_addr;
            destinationPort.value = inAddr->sin_port;

            if(inAddr->sin_addr.s_addr == INADDR_BROADCAST){
                return -ECONNREFUSED;
            }

            MACAddress mac;
            if(int e = Route(address, peerAddress, mac, adapter)){
                return e;
            }

            address = adapter->adapterIP;

            if(bound){
                ReleasePort();

                if(int e = AcquirePort(port); e){ // Re-acquire port now that we have destination address and port
                    return e;
                }
            } else {
                port = AllocatePort(); // Allocate ephemeral port

                if(!port){
                    Log::Warning("[TCPSocket] Failed to allocate temporary port!");
                    return -EADDRNOTAVAIL;
                }
            }

            state = TCPStateSyn;

            Log::Debug(debugLevelNetwork, DebugLevelVerbose, "[Network] [TCP] Connecting to %hd.%hd.%hd.%hd:%hd", peerAddress.data[0], peerAddress.data[1], peerAddress.data[2], peerAddress.data[3], (uint16_t)destinationPort);

            m_sequenceNumber = (Timer::GetSystemUptime() % 512) * (rand() % 255) + (Timer::UsecondsSinceBoot() % 255) + 1;
            Synchronize(m_sequenceNumber - 1); // The peer should acknowledge the sent sequence number + 1, so just send (sequenceNumber - 1)

            long retryPeriod = TCP_RETRY_MIN;
            while(state == TCPStateSyn){
                FilesystemBlocker bl(this);

                long timeout = retryPeriod;
                if(Thread::Current()->Block(&bl, timeout)){
                    return -EINTR;
                }

                if(timeout <= 0){
                    retryPeriod *= 4;
                }
            }

            if(state != TCPStateEstablished){
                return -ECONNREFUSED;
            }

            m_remoteSequenceNumber += 1;
            Acknowledge(m_remoteSequenceNumber);
            return 0;
        }

        int TCPSocket::Listen(int backlog){
            if(!bound){
                return -EDESTADDRREQ;
            }

            if(IsConnected()){
                return -EISCONN;
            }

            state = TCPStateListen;

            if(backlog > 0){
                pendingConnections = Semaphore(backlog - pending.get_length());
            }

            return 0;
        }

        int64_t TCPSocket::ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen, const void* ancillary, size_t ancillaryLen){
            if(state != TCPStateEstablished && !m_inboundData.Pos()){
                Log::Debug(debugLevelNetwork, DebugLevelNormal, "TCPSocket::ReceiveFrom: Not connected!");
                return -ENOTCONN;
            }

            if(addrlen && *addrlen >= sizeof(sockaddr_in)){
                *reinterpret_cast<sockaddr_in*>(src) = {.sin_family = AF_INET, .sin_port = destinationPort, .sin_addr = {peerAddress.value}};

                *addrlen = sizeof(sockaddr_in);
            }

            if(state == TCPStateEstablished && m_inboundData.Pos() < len){ // We do not want to block when in CLOSE-WAIT
                FilesystemBlocker bl(this, len);

                if(Thread::Current()->Block(&bl)){
                    return -EINTR; // We were interrupted
                }
            }

            if(state == TCPStateUnknown){
                return -ECONNRESET; // If state is now unknown we recieved an RST
            }

            if(len > m_inboundData.Pos()){
                len = m_inboundData.Pos(); // We could have been unblocked earlier by a PSH or FIN

                if(!len){
                    return 0;
                }
            }

            return m_inboundData.Read(buffer, len);
        }

        int64_t TCPSocket::SendTo(void* buffer, size_t len, int flags, const sockaddr* dest, socklen_t addrlen, const void* ancillary, size_t ancillaryLen){
            if(state != TCPStateEstablished){
                Log::Debug(debugLevelNetwork, DebugLevelNormal, "TCPSocket::SendTo: Not connected!");
                return -ENOTCONN;
            }

            if(dest || addrlen){
                Log::Debug(debugLevelNetwork, DebugLevelNormal, "TCPSocket::SendTo: Already connected!");
                return -EISCONN; // dest is invalid
            }

            uint16_t shortLength = static_cast<uint16_t>(len);

            TCPHeader header;
            memset(&header, 0, sizeof(TCPHeader));

            header.srcPort = port;
            header.destPort = destinationPort;
            header.sequence = m_sequenceNumber;
            header.acknowledgementNumber = m_remoteSequenceNumber;

            header.dataOffset = sizeof(TCPHeader) / 4;

            header.windowSize = 65535;

            header.ack = 1;
            header.psh = 1;

            TCPPacket pack = { .header = header, .sequenceNumber = m_sequenceNumber, .length = shortLength, .data = new uint8_t[len]};
            memcpy(pack.data, buffer, shortLength);

            m_sequenceNumber += shortLength;

            int64_t ret = SendTCP(buffer, shortLength, address, peerAddress, header, adapter);
            if(ret >= 0){
                ScopedSpinLock acquired(m_unacknowledgedPacketsLock);
                m_unacknowledgedPackets.add_back(pack);
            }

            return ret;
        }

        int TCPSocket::SetSocketOptions(int level, int opt, const void* optValue, socklen_t optLength){
            if(level == IPPROTO_TCP){
                switch(opt){
                    case TCP_NODELAY:
                        if(optLength < sizeof(int)){
                            return -EFAULT; // need to be at least int size
                        }

                        m_noDelay = *reinterpret_cast<const int*>(optValue); // Disable 'Nagle's algorithm', we don't actualy implement this yet 
                        // Nagle's algorithm involves buffering output until we fill a packet
                        return 0;
                    default:
                        Log::Warning("TCPSocket::SetSocketOptions: Unknown option: %d", opt);
                        return -ENOPROTOOPT;
                }
            } else if(level == SOL_SOCKET && opt == SO_KEEPALIVE){
                Log::Warning("TCPSocket keepalive is ignored!");

                if(optLength < sizeof(int)){
                    return -EFAULT; // need to be at least int size
                }

                m_keepAlive = *reinterpret_cast<const int*>(optValue);
                return 0;
            }

            return IPSocket::SetSocketOptions(level, opt, optValue, optLength);
        }

        int TCPSocket::GetSocketOptions(int level, int opt, void* optValue, socklen_t* optLength){
            if(level == IPPROTO_TCP){
                switch(opt){
                    case TCP_NODELAY:
                        if(*optLength < sizeof(int)){
                            return -EFAULT; // Not big enough, callers job to check the memory space
                        }

                        *optLength = sizeof(int);
                        *reinterpret_cast<int*>(optValue) = m_noDelay;
                        return 0;
                    default:
                        Log::Warning("TCPSocket::GetSocketOptions: Unknown option: %d", opt);
                        return -ENOPROTOOPT;
                }
            } else if(level == SOL_SOCKET && opt == SO_KEEPALIVE){
                if(*optLength < sizeof(int)){
                    return -EINVAL; // Not big enough, callers job to check the memory space
                }

                *optLength = sizeof(int);
                *reinterpret_cast<int*>(optValue) = m_keepAlive;
                return 0;
            }

            return IPSocket::GetSocketOptions(level, opt, optValue, optLength);
        }

        void TCPSocket::Close(){
            handleCount--;

            if(handleCount <= 0){
                Log::Debug(debugLevelNetwork, DebugLevelVerbose, "Closing TCP socket...");

                m_fileClosed = true;

                if(state == TCPStateListen || state == TCPStateUnknown){
                    return; // No active connection
                }
                
                if(state == TCPStateSyn || state == TCPStateSynAck){
                    Reset(); // Connection has not been estabilished

                    return;
                }

                if(state == TCPStateEstablished){
                    state = TCPStateFinWait1;

                    FinishAcknowledge(m_remoteSequenceNumber);
                } else if(state == TCPStateCloseWait){
                    state = TCPStateLastAck;

                    Finish();
                }

                closedSockets.add_back(this);
            }
        }
    }
}