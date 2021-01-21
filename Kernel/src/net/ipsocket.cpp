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

int64_t IPSocket::OnReceive(void* buffer, size_t len){
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

	address = inetAddr->sin_addr.s_addr;

	port.value = inetAddr->sin_port; // Should already be big endian
	if(!port.value){
		port.value = AllocatePort();
	} else {
		return AcquirePort(port.value);
	}

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
		ReleasePort(port);
	}

	Socket::Close();
}