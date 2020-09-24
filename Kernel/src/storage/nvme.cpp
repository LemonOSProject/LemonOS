#include <pci.h>
#include <logging.h>

namespace NVMe{
    char* deviceName = "Generic NVMe Controller";

    void Initialize(){
        if(!PCI::FindGenericDevice(PCI_CLASS_STORAGE, PCI_SUBCLASS_NVM)){
            Log::Warning("NVMe Controller Not Found!");
            return;
        }

        Log::Info("Initializing NVMe Controller...");
        
    }
}