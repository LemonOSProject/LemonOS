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
		perror("[NetworkGovernor] Could not bind UDP to port 67: ");
		return -2;
	}

	dhcpServerAddress.sin_addr.s_addr = inet_addr("255.255.255.255"); // Broadcast
	dhcpServerAddress.sin_family = AF_INET;
	dhcpServerAddress.sin_port = htons(67);

	timespec t;
	clock_gettime(CLOCK_BOOTTIME, &t);

	DHCPHeader header;
	header.op = DHCPOpCodeDiscover;
	header.htype = DHCPHardwareTypeEthernet;
	header.hlen = 6; // MAC addresses are 6 bytes long
	header.hops = 0;
	header.xID = rand() * (t.tv_sec + t.tv_nsec); // Generate random id
	header.secs = 0;
	header.flags = DHCP_FLAG_BROADCAST;
	header.clientIP = 0; // Don't worry about endianess 0.0.0.0 is symmetrical
	header.yourIP = 0;
	header.serverIP = 0;
	header.gatewayIP = 0;
	memcpy(header.clientAddress, "\xde\xad\xbe\xef\x69\x69", 6); // TODO: ioctl() for getting MAC address
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

	for(;;){

	}
	//sendto(dhcpSocket, &header, sizeof(DHCPHeader), 0, (sockaddr*)&dhcpServerAddress, sizeof(sockaddr_in));
}