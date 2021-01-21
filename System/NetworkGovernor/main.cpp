#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <vector>

#include "dhcp.h"

int dhcpSocket;
sockaddr_in dhcpClientAddress;
sockaddr_in dhcpServerAddress;

void SendDHCPMessage(DHCPHeader& header, std::vector<void*>& options){
	uint8_t* headerOptions = header.options;

	for(void* p : options){
		DHCPOption<0>* opt = (DHCPOption<0>*)p;

		memcpy(headerOptions, opt, opt->length + sizeof(DHCPOption<0>));

		headerOptions += opt->length + sizeof(DHCPOption<0>);
	}

	*headerOptions = 0xFF; // End mark
	headerOptions++;

	sendto(dhcpSocket, &header, sizeof(DHCPHeader), 0, (sockaddr*)&dhcpServerAddress, sizeof(sockaddr_in));
}

int main(){
	dhcpSocket = socket(AF_INET, SOCK_DGRAM, 0);

	if(!dhcpSocket){
		perror("[NetworkGovernor] Could not open UDP socket: ");
		return -1;
	}

	dhcpClientAddress.sin_family = AF_INET;
	dhcpClientAddress.sin_addr.s_addr = 0;
	dhcpClientAddress.sin_port = htons(68);

	if(bind(dhcpSocket, (sockaddr*)&dhcpClientAddress, sizeof(sockaddr_in))){
		perror("[NetworkGovernor] Could not bind UDP to port 68");
		return -2;
	}

	dhcpServerAddress.sin_addr.s_addr = inet_addr("255.255.255.255"); // Broadcast
	dhcpServerAddress.sin_family = AF_INET;
	dhcpServerAddress.sin_port = htons(67);

	timespec t;
	clock_gettime(CLOCK_BOOTTIME, &t);

	DHCPHeader header;
	memset(&header, 0, sizeof(DHCPHeader));

	header.op = DHCPOpCodeDiscover;
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

	options.push_back((void*)&dhcpMessage);
	options.push_back((void*)&dhcpParameters);

	SendDHCPMessage(header, options);

	uint32_t subnetMask = 0;
	uint32_t defaultGateway = 0;
	uint32_t dnsServer = 0;

	DHCPHeader recvHeader;
	sockaddr_in recvAddr;
	socklen_t recvLen = sizeof(sockaddr_in);

	recvfrom(dhcpSocket, &recvHeader, sizeof(DHCPHeader), 0, reinterpret_cast<sockaddr*>(&recvAddr), &recvLen);
	printf("received dhcp response:\n\
			IP Address: %d.%d.%d.%d\n",
			recvHeader.yourIP & 0xff, (recvHeader.yourIP >> 8) & 0xff, (recvHeader.yourIP >> 16) & 0xff, (recvHeader.yourIP >> 24) & 0xff);

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

	header.clientIP = recvHeader.yourIP; // Request the IP from the offer
	header.serverIP = recvHeader.serverIP;

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

	options.clear();
	options = { &dhcpMessage, &dhcpIPRequest, &dhcpServer };

	SendDHCPMessage(header, options);

	recvfrom(dhcpSocket, &recvHeader, sizeof(DHCPHeader), 0, reinterpret_cast<sockaddr*>(&recvAddr), &recvLen);
	printf("received dhcp response:\n\
			IP Address: %d.%d.%d.%d\n\
			Subnet Mask: %d.%d.%d.%d\n\
			Default Gateway: %d.%d.%d.%d\n\
			DNS Server: %d.%d.%d.%d\n",
			recvHeader.yourIP & 0xff, (recvHeader.yourIP >> 8) & 0xff, (recvHeader.yourIP >> 16) & 0xff, (recvHeader.yourIP >> 24) & 0xff,
			subnetMask & 0xff, (subnetMask >> 8) & 0xff, (subnetMask >> 16) & 0xff, (subnetMask >> 24) & 0xff,
			defaultGateway & 0xff, (defaultGateway >> 8) & 0xff, (defaultGateway >> 16) & 0xff, (defaultGateway >> 24) & 0xff,
			dnsServer & 0xff, (dnsServer >> 8) & 0xff, (dnsServer >> 16) & 0xff, (dnsServer >> 24) & 0xff);

	for(;;){

	}
}