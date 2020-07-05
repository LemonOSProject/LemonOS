#include <net/net.h>
#include <net/networkadapter.h>
#include <net/dhcp.h>

#include <scheduler.h>
#include <logging.h>
#include <math.h>
#include <timer.h>

namespace Network::Interface{
	void OnReceiveUDP(IPv4Header ipHeader, void* data, size_t length){
		if(length < sizeof(UDPHeader)){
			Log::Warning("[Network] [UDP] Discarding packet (too short)");
			return;
		}

		UDPHeader* header = (UDPHeader*)data;
		
		Log::Info("[Network] [UDP] Receiving Packet (Source port: %d, Destination port: %d)", header->srcPort, header->destPort);
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

		switch(header->protocol){
			case IPv4ProtocolUDP:
				OnReceiveUDP(*header, header->data, length - sizeof(IPv4Header));
				break;
			case IPv4ProtocolTCP:
			default:
				Log::Warning("[Network] [IPv4] Discarding packet (invalid protocol %x)", header->protocol);
				break;
		}
	}

	[[noreturn]] void InterfaceProcess(){
		Log::Info("[Network] Initializing network interface layer...");

		assert(mainAdapter);

		while(mainAdapter->GetLink() != LinkUp) Scheduler::Yield();

		for(;;){
			NetworkPacket p;
			while((p = mainAdapter->Dequeue()).length){
				if(p.length < sizeof(EthernetFrame)){
					Log::Warning("[Network] Discarding packet (too short)");
					continue;
				}

				EthernetFrame* etherFrame = (EthernetFrame*)p.data;

				if(etherFrame->dest != mainAdapter->mac){
					Log::Warning("[Network] Discarding packet (invalid MAC address)");
				}
				
				switch (etherFrame->etherType)
				{
				case EtherTypeIPv4:
					OnReceiveIPv4(etherFrame->data, p.length - sizeof(EthernetFrame));
					break;
				default:
					Log::Warning("[Network] Discarding packet (invalid EtherType)");
					break;
				}
			}	
		}
	}

	void Initialize(){
		Scheduler::CreateProcess((void*)InterfaceProcess);
	}

	void Send(void* data, size_t length){
		mainAdapter->SendPacket(data, length);
	}
}