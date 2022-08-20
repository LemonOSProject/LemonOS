#include <Net/Net.h>
#include <Net/Adapter.h>

#include <Scheduler.h>
#include <Logging.h>
#include <Math.h>
#include <Timer.h>
#include <Errno.h>

#include <Objects/Service.h>
#include <Objects/Interface.h>

#define NET_INTERFACE_STACKSIZE 32768

namespace Network{
	extern HashMap<uint32_t, MACAddress> addressCache;
	extern Vector<NetworkAdapter*> adapters;

	Semaphore packetQueueSemaphore(0);
	FancyRefPtr<Process> netProcess;

	void OnReceiveARP(void* data, size_t length){
		if(length < sizeof(ARPHeader)){
			IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
				Log::Warning("[Network] [ARP] Discarding packet (too short)");
			});

			return;
		}

		ARPHeader* arp = reinterpret_cast<ARPHeader*>(data);

		if(arp->opcode == arp->ARPRequest){
			NetworkAdapter* adapter = NetFS::GetInstance()->FindAdapter(arp->destPrAddr.value); // Find corresponding adapter to ip address

			if(adapter){ // One of our adapters!
				IF_DEBUG(debugLevelNetwork >= DebugLevelVerbose, {
					Log::Info("[Network] [ARP] Who is %d.%d.%d.%d? %d.%d.%d.%d it is us at %x:%x:%x:%x:%x:%x!",
						arp->destPrAddr.data[0], arp->destPrAddr.data[1], arp->destPrAddr.data[2], arp->destPrAddr.data[3],
						arp->srcPrAddr.data[0], arp->srcPrAddr.data[1], arp->srcPrAddr.data[2], arp->srcPrAddr.data[3],
						adapter->mac.data[0], adapter->mac.data[1], adapter->mac.data[2], adapter->mac.data[3], adapter->mac.data[4], adapter->mac.data[5]);
				});

				uint8_t buffer[sizeof(EthernetFrame) + sizeof(ARPHeader)];
				
				EthernetFrame* ethFrame = reinterpret_cast<EthernetFrame*>(buffer);
				ethFrame->etherType = EtherTypeIPv4;
				ethFrame->src = adapter->mac;
				ethFrame->dest = arp->srcHwAddr;

				ARPHeader* reply = reinterpret_cast<ARPHeader*>(buffer + sizeof(EthernetFrame));
				reply->destHwAddr = arp->srcHwAddr;
				reply->srcHwAddr = adapter->mac;
				reply->hLength = 6;
				reply->hwType = 1; // Ethernet

				reply->destPrAddr = arp->srcPrAddr;
				reply->srcPrAddr = adapter->adapterIP.value;
				reply->pLength = 4;
				reply->prType = EtherTypeIPv4;

				reply->opcode = ARPHeader::ARPReply;

				Network::Send(buffer, sizeof(EthernetFrame) + sizeof(ARPHeader), adapter);
			}
		}

		// Insert into cache regardless of whether it was a reply or request
		addressCache.insert(arp->srcPrAddr.value, arp->srcHwAddr);
	}

	void OnReceiveICMP(void* data, size_t length){
		if(length < sizeof(ICMPHeader)){
			Log::Warning("[Network] [ICMP] Discarding packet (too short)");
			return;
		}

		ICMPHeader* header = (ICMPHeader*)data;
		Log::Info("[Network] [ICMP] Received packet, Type: %d, Code: %d", header->type, header->code);
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
				UDP::OnReceiveUDP(*header, header->data, ((uint16_t)header->length) - sizeof(IPv4Header));
				break;
			case IPv4ProtocolTCP:
				TCP::OnReceiveTCP(*header, header->data, ((uint16_t)header->length) - sizeof(IPv4Header));
				break;
			default:
				Log::Warning("[Network] [IPv4] Discarding packet (invalid protocol %x)", header->protocol);
				break;
		}
	}

	[[noreturn]] void InterfaceThread(){
		Log::Info("[Network] Initializing network interface layer...");

		for(;;){
			NetworkPacket* p;
			if(packetQueueSemaphore.Wait()){
				continue; // We got interrupted
			}
			
			for(NetworkAdapter* adapter : adapters){
				if((p = adapter->Dequeue())){
					if(p->length < sizeof(EthernetFrame)){
						Log::Warning("[Network] Discarding packet (too short)");

						adapter->CachePacket(p);
						break;
					}

					size_t len = p->length;
					uint8_t buffer[ETHERNET_MAX_PACKET_SIZE];
					memcpy(buffer, p->data, p->length);

					adapter->CachePacket(p); // Get the packet back to the NIC driver's cache as soon as possible

					EthernetFrame* etherFrame = reinterpret_cast<EthernetFrame*>(buffer);
					if(etherFrame->dest != adapter->mac && etherFrame->dest != MACAddress{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}){
						Log::Warning("[Network] Discarding packet (invalid MAC address %x:%x:%x:%x:%x:%x)", etherFrame->dest[0], etherFrame->dest[1], etherFrame->dest[2], etherFrame->dest[3], etherFrame->dest[4], etherFrame->dest[5]);
						continue;
					}
					
					switch ((uint16_t)etherFrame->etherType)
					{
					case EtherTypeIPv4:
						OnReceiveIPv4(etherFrame->data, len - sizeof(EthernetFrame));
						break;
					case EtherTypeARP:
						OnReceiveARP(etherFrame->data, len - sizeof(EthernetFrame));
						break;
					default:
						Log::Warning("[Network] Discarding packet (invalid EtherType %x)", etherFrame->etherType);
						break;
					}
				}
			}
		}
	}

	void InitializeNetworkThread(){
		netProcess = Process::CreateKernelProcess((void*)InterfaceThread, "NetworkStack", nullptr);
		netProcess->Start();
	}

	void Send(void* data, size_t length, NetworkAdapter* adapter){
		if(adapter){
			adapter->SendPacket(data, length);
		} else for(auto& adapter : adapters){
			adapter->SendPacket(data, length);
			break;
		}
	}

    int SendIPv4(void* data, size_t length, IPv4Address& source, IPv4Address& destination, uint8_t protocol, NetworkAdapter* adapter){
		if(length > 1518 - sizeof(EthernetFrame) - sizeof(IPv4Header)){
			return -EMSGSIZE;
		}

		assert(adapter);

		uint8_t buffer[1600]; // The maxmium Ethernet frame size is 1518 so this will do

		EthernetFrame* ethFrame = (EthernetFrame*)buffer;
		ethFrame->etherType = EtherTypeIPv4;
		ethFrame->src = adapter->mac;
		
		if(destination.value == INADDR_BROADCAST){
			ethFrame->dest = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // Broadcast MAC Address
		} else if(int status = Route(source, destination, ethFrame->dest, adapter); status < 0){
			return status;
		}

		IPv4Header* ipHeader = (IPv4Header*)ethFrame->data;
		memset(ipHeader, 0, sizeof(IPv4Header));

		ipHeader->ihl = 5; // 5 dwords (20 bytes)
		ipHeader->version = 4; // Internet Protocol version 4
		ipHeader->length = length + sizeof(IPv4Header);
		ipHeader->flags = 0;
		ipHeader->ttl = 64;
		ipHeader->protocol = protocol;
		ipHeader->headerChecksum = 0;
		ipHeader->destIP = destination;
		ipHeader->sourceIP = adapter->adapterIP;

		ipHeader->headerChecksum = CaclulateChecksum(ipHeader, sizeof(IPv4Header));

		memcpy(ipHeader->data, data, length);

		Send(ethFrame, sizeof(EthernetFrame) + sizeof(IPv4Header) + length, adapter);

		return 0;
	}
}