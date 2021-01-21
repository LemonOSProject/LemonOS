#include <net/socket.h>

#include <hash.h>
#include <errno.h>
#include <math.h>
#include <debug.h>

namespace Network::UDP{
    HashMap<uint16_t, UDPSocket*> sockets;
    uint16_t nextEphemeralPort = EPHEMERAL_PORT_RANGE_START;

    Socket* FindSocket(BigEndian<uint16_t> port){
        return sockets.get(port.value);
    }

    int AcquirePort(UDPSocket* sock, unsigned int port){
        if(!port || port > PORT_MAX){
            Log::Warning("[Network] AcquirePort: Invalid port: %d", port);
            return -1;
        }

        if(sockets.get(port)){
            Log::Warning("[Network] AcquirePort: Port %d in use!", port);
            return -2;
        }

        sockets.insert(port, sock);
        
        return 0;
    }

    unsigned short AllocatePort(UDPSocket* sock){
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

    void OnReceiveUDP(IPv4Header& ipHeader, void* data, size_t length){
		if(length < sizeof(UDPHeader)){
			Log::Warning("[Network] [UDP] Discarding packet (too short)");
			return;
		}

		UDPHeader* header = (UDPHeader*)data;
        if(header->length > length){
			Log::Warning("[Network] [UDP] Discarding packet (too long)");
            return;
        }
		
		IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
		    Log::Info("[Network] [UDP] Receiving Packet (Source port: %d, Destination port: %d)", (uint16_t)header->srcPort, (uint16_t)header->destPort);
        });

        if(UDPSocket* sock = sockets.get(header->destPort.value); sock){
            sock->OnReceive(ipHeader.sourceIP, header->srcPort, header->data, header->length);
        }
    }

    UDPSocket::UDPSocket(int type, int protocol) : IPSocket(type, protocol){
        assert(type == DatagramSocket);
    }

    unsigned short UDPSocket::AllocatePort(){
        return Network::UDP::AllocatePort(this);
    }

    int UDPSocket::AcquirePort(uint16_t port){
        return Network::UDP::AcquirePort(this, port);
    }

    int UDPSocket::ReleasePort(uint16_t port){
        return Network::UDP::ReleasePort(port);
    }

    int64_t UDPSocket::OnReceive(IPv4Address& sourceIP, BigEndian<uint16_t> sourcePort, void* buffer, size_t len){
        UDPPacket pkt;
        pkt.sourceIP = sourceIP;
        pkt.sourcePort = sourcePort;
        pkt.length = len;

        pkt.data = new uint8_t[len];
        memcpy(pkt.data, buffer, len);

        packets.add_back(pkt);

        acquireLock(&waitingLock);
        while(waiting.get_length()){
            Scheduler::UnblockThread(waiting.remove_at(0));
        }
        releaseLock(&waitingLock);

        return 0;
    }

    Socket* UDPSocket::Accept(sockaddr* addr, socklen_t* addrlen, int mode){
        return nullptr;
    }

    int UDPSocket::Bind(const sockaddr* addr, socklen_t addrlen){
        
        if(int e = IPSocket::Bind(addr, addrlen)){
            if(e == -2){
                return -EADDRINUSE; // Failed to acquire port
            }
        }

        assert(port && port < PORT_MAX);

        return 0;
    }

    int UDPSocket::Connect(const sockaddr* addr, socklen_t addrlen){
        return -EOPNOTSUPP;
    }

    int UDPSocket::Listen(int backlog){
        return -EOPNOTSUPP;
    }

    int64_t UDPSocket::ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen){
        if(!packets.get_length()){
            if(flags & MSG_DONTWAIT){
                return -EAGAIN; // Don't wait
            } else {
                Scheduler::BlockCurrentThread(waiting, waitingLock);
            }
        }

        acquireLock(&packetsLock);
        if(packets.get_length() <= 0){
                releaseLock(&packetsLock);
            if(flags & MSG_DONTWAIT){
                return -EAGAIN; // Don't wait
            } else while(packets.get_length() <= 0){
                Scheduler::Yield();
            }
        }

        UDPPacket pkt = packets.remove_at(0);
        releaseLock(&packetsLock);

        if(src && addrlen){
            sockaddr_in addr;
            addr.sin_family = InternetProtocol;
            addr.sin_port = pkt.sourcePort;
            addr.sin_addr.s_addr = pkt.sourceIP.value;

            memcpy(src, &addr, *addrlen); // Make sure to stay within bounds of addrlen

            *addrlen = sizeof(sockaddr_in); // addrlen is updated to contain the actual size of the source address
        }

        size_t finalLength = MIN(len, pkt.length);
        memcpy(buffer, pkt.data, finalLength);

        delete[] pkt.data; // Free buffer

        return finalLength;
    }

    int64_t UDPSocket::SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen){
        IPv4Address sendIPAddress;
        BigEndian<uint16_t> destPort;

        if(src){
            const sockaddr_in* inetAddr = (const sockaddr_in*)src;

            if(src->family != InternetProtocol){
                Log::Warning("[UDPSocket] Invalid address family (not IPv4)");
                return -EINVAL;
            }
            
            if(addrlen < sizeof(sockaddr_in)){
                Log::Warning("[UDPSocket] Invalid address length");
                return -EINVAL;
            }

            sendIPAddress = inetAddr->sin_addr.s_addr;
            destPort.value = inetAddr->sin_port; // Should already be big endian
        } else {
            sendIPAddress = address;
            destPort = destinationPort;
        }

        if(!port){
            port = AllocatePort();

            if(!port){
                Log::Warning("[UDPSocket] Failed to allocate temporary port!");
                return -1;
            }
        }

        if(int e = Network::Interface::SendUDP(buffer, len, sendIPAddress, port, destPort)){
            return e;
        }

        return len;
    }
}