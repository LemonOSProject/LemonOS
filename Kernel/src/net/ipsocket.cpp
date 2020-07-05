#include <net/net.h>
#include <net/socket.h>
#include <errno.h>

IPSocket::IPSocket(int type, int protocol) : Socket(type, protocol) {
	domain = InternetProtocol;
	this->type = type;
}

Socket* IPSocket::Accept(sockaddr* addr, socklen_t* addrlen, int mode){
	return nullptr;
}

int IPSocket::Bind(const sockaddr* addr, socklen_t addrlen){
	return -ENOSYS;
}

int IPSocket::Connect(const sockaddr* addr, socklen_t addrlen){
	return -ENOSYS;
}

int IPSocket::Listen(int backlog){
	return -ENOSYS;
}

void IPSocket::Close(){
	

	Socket::Close();
}