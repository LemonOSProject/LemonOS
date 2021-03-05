#include <Audio/HDAudio.h>

#include <Pair.h>

namespace Audio {
	static const Pair<uint16_t, uint16_t> controllerPCIIDs[] = {

	};

	void IntelHDAudioController::Initialize(){
		(void)(cRegs);
		(void)controllerPCIIDs;
	}
}