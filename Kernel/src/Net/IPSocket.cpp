#include <Net/Net.h>
#include <Net/Socket.h>
#include <Net/Adapter.h>

#include <Errno.h>
#include <Logging.h>

IPSocket::IPSocket(int type, int protocol) : Socket(type, protocol) {
	domain = InternetProtocol;
	this->type = type;
}

IPSocket::~IPSocket(){
	
}

int IPSocket::Ioctl(uint64_t cmd, uint64_t arg){
	if(adapter){
		return adapter->Ioctl(cmd, arg); // Adapter ioctls
	} else {
		return -ENODEV; // We are not bound to an interface
	}
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

	if(inetAddr->sin_addr.s_addr == INADDR_ANY){
		if(adapter){
			adapter->UnbindSocket(this);
		}

		adapter = nullptr;
		address.value = INADDR_ANY;
	} else if(Network::NetworkAdapter* a = Network::NetFS::GetInstance()->FindAdapter(inetAddr->sin_addr.s_addr); a){
		a->BindToSocket(this);
	} else {
		return -EADDRNOTAVAIL; // Address does not exist
	}

	address = inetAddr->sin_addr.s_addr;

	port.value = inetAddr->sin_port; // Should already be big endian
	if(!port.value){
		port = AllocatePort();
	}
	
	return AcquirePort((uint16_t)port);
}

int IPSocket::Connect(const sockaddr* addr, socklen_t addrlen){
	return -ENOSYS;
}

int IPSocket::Listen(int backlog){
	return -ENOSYS;
}
    
int64_t IPSocket::ReceiveFrom(void* buffer, size_t len, int flags, sockaddr* src, socklen_t* addrlen, const void* ancillary, size_t ancillaryLen){
	if(flags & SOCK_NONBLOCK && !pQueue.get_length()){
		return -EAGAIN;
	}

	return -ENOSYS;

	while(!pQueue.get_length()) Scheduler::Yield();

	if(src && addrlen){
		src->family = InternetProtocol;
	}
}

int64_t IPSocket::SendTo(void* buffer, size_t len, int flags, const sockaddr* src, socklen_t addrlen, const void* ancillary, size_t ancillaryLen){
	return -ENOSYS;
}

int IPSocket::SetSocketOptions(int level, int opt, const void* optValue, socklen_t optLength){
	if(level == SOL_IP){
		switch(opt){
		case IP_PKTINFO:
			if(type != DatagramSocket){
				return -ENOPROTOOPT;
			}

			if(optLength >= sizeof(int32_t)){
				pktInfo = *(int32_t*)optValue;
			} else {
				return -EINVAL;
			}
			break;
		default:
			return -ENOPROTOOPT;
		}
	} else if(level == SOL_SOCKET && opt == SO_BINDTODEVICE) {
		const char* req = reinterpret_cast<const char*>(optValue);
		
		Network::NetworkAdapter* a = Network::NetFS::GetInstance()->FindAdapter(req, optLength);

		if(a){
			a->BindToSocket(this);
		} else {
			return -EINVAL; // No such interface found
		}

		return 0;
	} else if(level == SOL_SOCKET && opt == SO_BINDTOIFINDEX) {
		if(optLength >= sizeof(ifreq)){
			Log::Warning("IPSocket SO_BINDTOIFINDEX unimplemented!");
			return -ENOSYS; // TODO: Bind to interface index
		} else {
			return -EINVAL;
		}
	} else {
		return Socket::SetSocketOptions(level, opt, optValue, optLength);
	}
	
	return 0;
}

int IPSocket::GetSocketOptions(int level, int opt, void* optValue, socklen_t* optLength){
	if(level == SOL_IP){
		Log::Warning("IPSocket GetSocketOptions unimplemented!");
		return -ENOSYS;
	}

	return Socket::GetSocketOptions(level, opt, optValue, optLength);
}

void IPSocket::Close(){
	if(port){
		ReleasePort();
	}

	if(adapter){
		adapter->UnbindSocket(this);
	}

	Socket::Close();
}