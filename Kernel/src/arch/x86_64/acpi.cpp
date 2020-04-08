#include <acpi.h>

#include <string.h>
#include <logging.h>
#include <panic.h>
/*
#include <lai/core.h>
#include <lai/helpers/pm.h>
#include <lai/helpers/sci.h>*/

namespace ACPI{
	/*const char* signature = "RSD PTR ";

	RSDPDescriptor desc;
	RSDTHeader header;*/

	char oem[7];

	void Init(){
		/*for(int i = 0; i <= 0x400; i++){ // Search first KB for RSDP
			if(memcmp((void*)i,signature,8) == 0){
				desc = *((RSDPDescriptor*)i);

				goto success;
			}
		}

		for(int i = 0xE0000; i <= 0xFFFFF; i++){ // Search further for RSDP
			if(memcmp((void*)i,signature,8) == 0){
				desc = *((RSDPDescriptor*)i);

				goto success;
			}
		}

		{
		const char* panicReasons[]{"System not ACPI Complaiant."};
		KernelPanic(panicReasons,1);
		}

		success:

		header = *((RSDTHeader*)desc.rsdtAddress);

		memcpy(oem,header.oemID,6);
		oem[6] = 0; // Zero OEM String

		Log::Info("ACPI Revision: ");
		Log::Info(header.revision, false);
		Log::Info("OEM ID: ");
		Log::Write(oem);

		lai_set_acpi_revision(header.revision);
		lai_enable_acpi(0);*/
	}

	void Reset(){
		//lai_acpi_reset();
	}
}