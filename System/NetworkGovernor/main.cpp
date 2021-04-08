#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <arpa/inet.h>

#include <linux/route.h>
#include <linux/sockios.h>

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <vector>
#include <string>
#include <string_view>

#include <Lemon/System/Module.h>

#include "dhcp.h"

sockaddr_in dhcpClientAddress;
sockaddr_in dhcpServerAddress;
int dhcpResponseTimeout = 8; // 8 seconds

void SendDHCPMessage(int sock, DHCPHeader& header, std::vector<void*>& options){
	uint8_t* headerOptions = header.options;

	for(void* p : options){
		DHCPOption<0>* opt = (DHCPOption<0>*)p;

		memcpy(headerOptions, opt, opt->length + sizeof(DHCPOption<0>));

		headerOptions += opt->length + sizeof(DHCPOption<0>);
	}

	*headerOptions = 0xFF; // End mark
	headerOptions++;

	sendto(sock, &header, sizeof(DHCPHeader), 0, (sockaddr*)&dhcpServerAddress, sizeof(sockaddr_in));
}

class NetworkInterface{
	std::string_view name;
	int sock;
	bool leaseAcquired = false;

public:
	uint32_t interfaceIP;
	uint32_t defaultGateway;
	uint32_t subnetMask;
	uint32_t dnsServer;

	enum InterfaceState {
		InterfaceUninitialized,
		InterfaceInitialized,
	};

	std::vector<uint32_t> dnsServers;
	NetworkInterface(const char* ifName, int ifSocket){
		name = strdup(ifName);
		sock = ifSocket;

		printf("Initializing interface %s!\n", ifName);
	}

	void AcquireLease(){
		timespec t;
		clock_gettime(CLOCK_BOOTTIME, &t);

		DHCPHeader header;
		memset(&header, 0, sizeof(DHCPHeader));

		header.op = BootPRequest;
		header.htype = DHCPHardwareTypeEthernet;
		header.hlen = 6; // MAC addresses are 6 bytes long
		header.hops = 0;
		header.xID = rand() * ((t.tv_sec << 1) ^ t.tv_nsec); // Generate random id
		header.secs = 0;
		header.flags = DHCP_FLAG_BROADCAST;
		header.clientIP = 0; // Don't worry about endianess 0.0.0.0 is symmetrical
		header.yourIP = 0;
		header.serverIP = 0;
		header.gatewayIP = 0;
		memcpy(header.clientAddress, "\xde\xad\x69\xbe\xef\x42", 6); // TODO: ioctl() for getting MAC address
		header.cookie = DHCP_MAGIC_COOKIE;

		std::vector<void*> options;

		DHCPOption<1> dhcpMessage;
		dhcpMessage.length = 1;
		dhcpMessage.optionCode = DHCPOptionMessageType;
		dhcpMessage.data[0] = DHCPMessageTypeDiscover;

		DHCPOption<3> dhcpParameters;
		dhcpParameters.length = 3;
		dhcpParameters.optionCode = DHCPOptionParameterRequestList;
		dhcpParameters.data[0] = DHCPOptionSubnetMask;
		dhcpParameters.data[1] = DHCPOptionDefaultGateway;
		dhcpParameters.data[2] = DHCPOptionDNS;

		options = { &dhcpMessage, &dhcpParameters };

		SendDHCPMessage(sock, header, options);

		DHCPHeader recvHeader;
		sockaddr_in recvAddr;
		socklen_t recvLen = sizeof(sockaddr_in);

		ssize_t len = recvfrom(sock, &recvHeader, sizeof(DHCPHeader), 0, reinterpret_cast<sockaddr*>(&recvAddr), &recvLen);
		while(len < 0){
			if(errno != EAGAIN && errno != EWOULDBLOCK){
				fprintf(stderr, "Failed to acquire DHCP lease for %s, recvfrom error: %s\n", this->name.data(), strerror(errno));
				return;
			}

			timespec currentTime;
			clock_gettime(CLOCK_BOOTTIME, &currentTime);
			if(currentTime.tv_sec - t.tv_sec >= dhcpResponseTimeout){
				fprintf(stderr, "Failed to acquire DHCP lease for %s: timed out waiting for OFFER after %i seconds\n", this->name.data(), dhcpResponseTimeout);
				return;
			}

			usleep(100000); // Wait 100 ms for response

			len = recvfrom(sock, &recvHeader, sizeof(DHCPHeader), 0, reinterpret_cast<sockaddr*>(&recvAddr), &recvLen);
		}

		if(recvHeader.cookie != DHCP_MAGIC_COOKIE){
			fprintf(stderr, "Failed to acquire DHCP lease for %s, invalid magic cookie: %x\n", this->name.data(), (uint32_t)recvHeader.cookie);
			return;
		}

		if(recvHeader.htype != 1){ // Check for Ethernet HTYPE
			fprintf(stderr, "Failed to acquire DHCP lease for %s, invalid HTYPE: %x\n", this->name.data(), recvHeader.htype);
			return;
		}

		if(recvHeader.op != BootPReply){ // Check for Ethernet HTYPE
			fprintf(stderr, "Failed to acquire DHCP lease for %s, expected BOOTPREPLY, received opcode %x\n", this->name.data(), recvHeader.op);
			return;
		}

		clock_gettime(CLOCK_BOOTTIME, &t);

		header.clientIP = recvHeader.yourIP; // Request the IP from the offer
		header.serverIP = recvHeader.serverIP;

		header.flags = 0;

		DHCPOption<0>* option = reinterpret_cast<DHCPOption<0>*>(recvHeader.options);
		while(option->optionCode != 0xff /* end marker */ && reinterpret_cast<void*>(option) < recvHeader.options + 312){
			switch(option->optionCode){
				case DHCPOptionSubnetMask:
					if(option->length == 4 /* IP address length */){
						subnetMask = *reinterpret_cast<uint32_t*>(option->data);
					}
					break;
				case DHCPOptionDefaultGateway:
					if(option->length == 4 /* IP address length */){
						defaultGateway = *reinterpret_cast<uint32_t*>(option->data);
					}
					break;
				case DHCPOptionDNS:
					if(option->length == 4 /* IP address length */){
						dnsServer = *reinterpret_cast<uint32_t*>(option->data);
					}
					break;
			}

			option = reinterpret_cast<DHCPOption<0>*>(option->data + option->length);
		} 

		dhcpMessage.length = 1;
		dhcpMessage.optionCode = DHCPOptionMessageType;
		dhcpMessage.data[0] = DHCPMessageTypeRequest;

		DHCPOption<4> dhcpIPRequest;
		dhcpIPRequest.length = 4;
		dhcpIPRequest.optionCode = DHCPOptionRequestIPAddress;
		*reinterpret_cast<uint32_t*>(dhcpIPRequest.data) = recvHeader.yourIP; // Request the IP from the offer

		DHCPOption<4> dhcpServer;
		dhcpServer.length = 4;
		dhcpServer.optionCode = DHCPOptionServerIdentifier;
		*reinterpret_cast<uint32_t*>(dhcpServer.data) = recvHeader.serverIP; // Use the server IP from the offer

		options = { &dhcpMessage, &dhcpIPRequest, &dhcpServer };

		SendDHCPMessage(sock, header, options);

		len = recvfrom(sock, &recvHeader, sizeof(DHCPHeader), 0, reinterpret_cast<sockaddr*>(&recvAddr), &recvLen);
		while(len < 0){
			if(errno != EAGAIN && errno != EWOULDBLOCK){
				fprintf(stderr, "Failed to acquire DHCP lease for %s, recvfrom error: %s\n", this->name.data(), strerror(errno));
				return;
			}

			timespec currentTime;
			clock_gettime(CLOCK_BOOTTIME, &currentTime);
			if(currentTime.tv_sec - t.tv_sec >= dhcpResponseTimeout){
				fprintf(stderr, "Failed to acquire DHCP lease for %s: timed out waiting for ACK after %i seconds\n", this->name.data(), dhcpResponseTimeout);
				return;
			}

			usleep(100000); // Wait 100 ms for response

			len = recvfrom(sock, &recvHeader, sizeof(DHCPHeader), 0, reinterpret_cast<sockaddr*>(&recvAddr), &recvLen);
		}

		interfaceIP = recvHeader.yourIP;
		leaseAcquired = true;

		printf("received dhcp response:\n\
IP Address: %d.%d.%d.%d\n\
Subnet Mask: %d.%d.%d.%d\n\
Default Gateway: %d.%d.%d.%d\n\
DNS Server: %d.%d.%d.%d\n",
				interfaceIP & 0xff, (interfaceIP >> 8) & 0xff, (interfaceIP >> 16) & 0xff, (interfaceIP >> 24) & 0xff,
				subnetMask & 0xff, (subnetMask >> 8) & 0xff, (subnetMask >> 16) & 0xff, (subnetMask >> 24) & 0xff,
				defaultGateway & 0xff, (defaultGateway >> 8) & 0xff, (defaultGateway >> 16) & 0xff, (defaultGateway >> 24) & 0xff,
				dnsServer & 0xff, (dnsServer >> 8) & 0xff, (dnsServer >> 16) & 0xff, (dnsServer >> 24) & 0xff);
	}

	void ConfigureInterface(){
		if(!leaseAcquired){
			AcquireLease();

			if(!leaseAcquired){
				return; // Failed to acquire lease
			}
		}

		ifreq ifRequest;

		strncpy(ifRequest.ifr_name, name.data(), 16);

		ifRequest.ifr_netmask.sa_family = AF_INET;
		reinterpret_cast<sockaddr_in*>(&ifRequest.ifr_netmask)->sin_addr.s_addr = subnetMask;
		ioctl(sock, SIOCSIFNETMASK, &ifRequest);

		rtentry route;
		route.rt_gateway.sa_family = AF_INET;
		route.rt_flags = RTF_GATEWAY | RTF_UP;
		reinterpret_cast<sockaddr_in*>(&route.rt_gateway)->sin_addr.s_addr = defaultGateway;
		route.rt_dev = const_cast<char*>(name.data());
		ioctl(sock, SIOCADDRT, &route);

		ifRequest.ifr_addr.sa_family = AF_INET;
		reinterpret_cast<sockaddr_in*>(&ifRequest.ifr_addr)->sin_addr.s_addr = interfaceIP;
		ioctl(sock, SIOCSIFADDR, &ifRequest);
	}

	~NetworkInterface(){
		close(sock);
	}
};

std::vector<NetworkInterface*> interfaces;
int main(){
	Lemon::LoadKernelModule("/initrd/modules/e1k.sys");

	DIR* netFS = opendir("/dev/net");
	if(!netFS){
		perror("Error opening /dev/net");
		return 1;
	}

	dhcpClientAddress.sin_family = AF_INET;
	dhcpClientAddress.sin_addr.s_addr = INADDR_ANY;
	dhcpClientAddress.sin_port = htons(68);

	dhcpServerAddress.sin_addr.s_addr = inet_addr("255.255.255.255"); // Broadcast
	dhcpServerAddress.sin_family = AF_INET;
	dhcpServerAddress.sin_port = htons(67);

	while(dirent* entry = readdir(netFS)){
		if(strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0){
			continue; // Ignore . and ..
		}

		int sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
		if(sock < 0){
			perror("Error creating UDP socket");
			continue;
		}

		if(bind(sock, (sockaddr*)&dhcpClientAddress, sizeof(sockaddr_in))){
			perror("[NetworkGovernor] Could not bind UDP to port 68");
			return 1;
		}

		int status = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, entry->d_name, strlen(entry->d_name));
		if(status){
			fprintf(stderr, "Error binding to interface '%s': %s\n", entry->d_name, strerror(errno));

			close(sock);
			continue;
		}

		interfaces.push_back(new NetworkInterface(entry->d_name, sock));
	}

	for(auto& netIf : interfaces){
		netIf->ConfigureInterface();
	}

	return 0;
}