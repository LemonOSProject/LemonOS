#include <iostream>
#include <vector>

#include <lemon/core/url.h>

#include <sys/socket.h>
#include <arpa/inet.h>

int main(int argc, char** argv){
	if(argc < 2){
		std::cout << "Usage: " << argv[0] << "[options] <url>\nSee " << argv[0] << "--help";
		return 1;
	}

	Lemon::URL url(argv[1]);

	if(!url.IsValid() || !url.Host().length()){
		std::cout << "steal: Invalid/malformed URL";
		return 2;
	}

	uint32_t ip;
	if(inet_pton(AF_INET, url.Host().c_str(), &ip) > 0){

	} else {
		return -1; // No DNS support yet
	}

	int sock = socket(AF_INET, SOCK_STREAM, 0);

	sockaddr_in address;
	address.sin_addr.s_addr = ip;
	address.sin_family = AF_INET;
	address.sin_port = htons(80);

	if(int e = connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_in)); e){
		perror("steal: Failed to connect");
		return 3;
	}

	std::ostream& out = std::cout;

	char request[4096];
	int reqLength = snprintf(request, 4096, "GET / HTTP/1.1\r\n\
Host: %s\r\n\
User-Agent: steal/0.1\r\n\
Accept: */*\r\n\
\r\n", url.Host().c_str());

	if(ssize_t len = send(sock, request, reqLength, 0); len != reqLength){
		if(reqLength < 0){
			perror("steal: Failed to send data");
		} else {
			std::cout << "steal: Failed to send all " << reqLength << " bytes (only sent " << len << ").\n";
		}

		return 4;
	}

	char receiveBuffer[4096];
	while(ssize_t len = recv(sock, receiveBuffer, 4096, 0)){
		if(len < 0){
			perror("steal: Failed stealing data");
			return 5;
		}

		out.write(receiveBuffer, len);

		if(len < 4096){
			break; // No more data to recieve
		}
	} 

	return 0;
}
