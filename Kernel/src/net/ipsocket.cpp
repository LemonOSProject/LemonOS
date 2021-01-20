#include <net/net.h>
#include <net/socket.h>
#include <errno.h>
#include <logging.h>

IPSocket::IPSocket(int type, int protocol) : Socket(type, protocol) {
	domain = InternetProtocol;
	this->type = type;
}

IPSocket::~IPSocket(){
	
}

int64_t IPSocket::IPReceiveFrom(void* buffer, size_t len){
	return -ENOSYS;
}

Socket* IPSocket::Accept(sockaddr* addr, socklen_t* addrlen, int mode){
	return nullptr;
}

int IPSocket::Bind(const sockaddr* addr, socklen_t addrlen){
	const sockaddr_in* inetAddr = (const sockaddr_in*)addr;

	if(addr->family != InternetProtocol){
		Log::Warning("[UDPSocket] Invalid address family (not IPv4)");
		return -EINVAL;
	}
	
	if(addrlen < sizeof(sockaddr_in)){
		Log::Warning("[UDPSocket] Invalid address length");
		return -EINVAL;
	}

	address = inetAddr->in_addr.s_addr;
	port.value = inetAddr->sin_port; // Should already be big endian

	return 0;
}

int IPSocket::Connect(const sockaddr* addr, socklen_t addrlen){
	return -ENOSYS;
}

int IPSocket::Listen(int backlog){
	return -ENOSYS;
}
    
int64_t IPSocket::ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen){
	if(flags & SOCK_NONBLOCK && !pQueue.get_length()){
		return -EAGAIN;
	}

	return -ENOSYS;

	while(!pQueue.get_length()) Scheduler::Yield();

	if(src && addrlen){
		src->family = InternetProtocol;
	}
}

int64_t IPSocket::SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen){
	return -ENOSYS;
}

void IPSocket::Close(){
	if(port){
		Network::ReleasePort(port);
	}

	Socket::Close();
}

UDPSocket::UDPSocket(int type, int protocol) : IPSocket(type, protocol){
	assert(type == DatagramSocket);
}

int64_t UDPSocket::IPReceiveFrom(void* buffer, size_t len){
	return -ENOSYS;
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

		sendIPAddress = inetAddr->in_addr.s_addr;
		destPort.value = inetAddr->sin_port; // Should already be big endian
	} else {
		sendIPAddress = address;
		destPort = destinationPort;
	}

	if(!port){
		port = Network::AllocatePort(*this);

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