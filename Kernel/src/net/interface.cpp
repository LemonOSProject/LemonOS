#include <net/net.h>
#include <net/networkadapter.h>

#include <scheduler.h>
#include <logging.h>
#include <math.h>
#include <timer.h>
#include <errno.h>

#include <objects/service.h>
#include <objects/interface.h>

#define NET_INTERFACE_STACKSIZE 32768

namespace Network::Interface{
	IPv4Address gateway = {0, 0, 0, 0};
	IPv4Address subnet = {255, 255, 255, 255};

	MACAddress IPLookup(IPv4Address& ip){
		Log::Warning("[Network] IPLookup not implemented");
		return {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};	
	}

	void OnReceiveICMP(void* data, size_t length){
		if(length < sizeof(ICMPHeader)){
			Log::Warning("[Network] [ICMP] Discarding packet (too short)");
			return;
		}

		ICMPHeader* header = (ICMPHeader*)data;
		Log::Info("[Network] [ICMP] Received packet, Type: %d, Code: %d", header->type, header->code);
	}

	void OnReceiveUDP(IPv4Header& ipHeader, void* data, size_t length){
	}

    void OnReceiveIPv4(void* data, size_t length){
		if(length < sizeof(IPv4Header)){
			Log::Warning("[Network] [IPv4] Discarding packet (too short)");
			return;
		}

		IPv4Header* header = (IPv4Header*)data;

		if(header->version != 4){
			Log::Warning("[Network] [IPv4] Discarding packet (invalid version)");
			return;
		}

		BigEndian<uint16_t> checksum = header->headerChecksum;

		header->headerChecksum = 0;
		if(checksum.value != CaclulateChecksum(data, sizeof(IPv4Header)).value){ // Verify checksum
			Log::Warning("[Network] [IPv4] Discarding packet (invalid checksum)");
			return;
		}

		switch(header->protocol){
			case IPv4ProtocolICMP:
				OnReceiveICMP(header->data, length - sizeof(IPv4Header));
				break;
			case IPv4ProtocolUDP:
				UDP::OnReceiveUDP(*header, header->data, length - sizeof(IPv4Header));
				break;
			case IPv4ProtocolTCP:
			default:
				Log::Warning("[Network] [IPv4] Discarding packet (invalid protocol %x)", header->protocol);
				break;
		}
	}

	[[noreturn]] void InterfaceThread(){
		Log::Info("[Network] Initializing network interface layer...");

		assert(mainAdapter);

		for(;;){
			NetworkPacket* p;
			while((p = mainAdapter->DequeueBlocking())){
				IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
					Log::Info("got packet!");
				});
				
				if(p->length < sizeof(EthernetFrame)){
					Log::Warning("[Network] Discarding packet (too short)");

					mainAdapter->CachePacket(p);
					continue;
				}

				size_t len = p->length;
				uint8_t buffer[ETHERNET_MAX_PACKET_SIZE];
				memcpy(buffer, p->data, p->length);

				mainAdapter->CachePacket(p); // Get the packet back to the NIC driver's cache as soon as possible

				EthernetFrame* etherFrame = reinterpret_cast<EthernetFrame*>(buffer);

				if(etherFrame->dest != mainAdapter->mac){
					//Log::Warning("[Network] Discarding packet (invalid MAC address %x:%x:%x:%x:%x:%x)", etherFrame->dest[0], etherFrame->dest[1], etherFrame->dest[2], etherFrame->dest[3], etherFrame->dest[4], etherFrame->dest[5]);
				}
				
				switch (etherFrame->etherType)
				{
				case EtherTypeIPv4:
					OnReceiveIPv4(etherFrame->data, len - sizeof(EthernetFrame));
					break;
				default:
					Log::Warning("[Network] Discarding packet (invalid EtherType %x)", etherFrame->etherType);
					break;
				}
			}
		}
	}

	void Initialize(){
		//Scheduler::CreateChildThread(Scheduler::GetCurrentProcess(), (uintptr_t)InterfaceThread, (uintptr_t)kmalloc(NET_INTERFACE_STACKSIZE) + NET_INTERFACE_STACKSIZE, KERNEL_CS, KERNEL_SS);
		Scheduler::CreateProcess((void*)InterfaceThread);
	}

	void Send(void* data, size_t length, NetworkAdapter* adapter){
		if(adapter)
			adapter->SendPacket(data, length);
		else if(mainAdapter)
			mainAdapter->SendPacket(data, length);
	}

    int SendIPv4(void* data, size_t length, IPv4Address& destination, uint8_t protocol, NetworkAdapter* adapter){
		if(length > 1518 - sizeof(EthernetFrame) - sizeof(IPv4Header)){
			return -EMSGSIZE;
		}

		uint8_t buffer[1600]; // The maxmium Ethernet frame size is 1518 so this will do

		EthernetFrame* ethFrame = (EthernetFrame*)buffer;
		ethFrame->etherType = EtherTypeIPv4;
		ethFrame->src = mainAdapter->mac;
		ethFrame->dest = IPLookup(destination);

		IPv4Header* ipHeader = (IPv4Header*)ethFrame->data;
		ipHeader->ihl = 5; // 5 dwords (20 bytes)
		ipHeader->version = 4; // Internet Protocol version 4
		ipHeader->length = length + sizeof(IPv4Header);
		ipHeader->flags = 0;
		ipHeader->ttl = 64;
		ipHeader->protocol = protocol;
		ipHeader->headerChecksum = 0;
		ipHeader->destIP = destination;
		ipHeader->sourceIP = gateway;

		ipHeader->headerChecksum = CaclulateChecksum(ipHeader, sizeof(IPv4Header));

		memcpy(ipHeader->data, data, length);

		Send(ethFrame, sizeof(EthernetFrame) + sizeof(IPv4Header) + length, adapter);

		return 0;
	}

    int SendUDP(void* data, size_t length, IPv4Address& destination, BigEndian<uint16_t> sourcePort, BigEndian<uint16_t> destinationPort, NetworkAdapter* adapter){
		if(length > 1518){
			return -EMSGSIZE;
		}

		uint8_t buffer[1600];

		UDPHeader* header = (UDPHeader*)buffer;
		header->destPort = destinationPort;
		header->srcPort = sourcePort;
		header->length = sizeof(UDPHeader) + length;
		header->checksum = 0;

		memcpy(header->data, data, length);

		//header->checksum = CaclulateChecksum(header, sizeof(UDPHeader));

		return SendIPv4(header, header->length, destination, IPv4ProtocolUDP, adapter);
	}
}