#include <pci.h>
#include <logging.h>

namespace NVMe{
    pci_device_t device;
    char* deviceName = "Generic NVMe Controller";

    void Initialize(){
        device.classCode = PCI_CLASS_STORAGE;
        device.subclass = 0x08; // Non-volatile memory controller subclass
        device.generic = true;
        device.deviceName = deviceName;

        device = PCI::RegisterPCIDevice(device);

        if(device.vendorID == 0xFFFF){
            Log::Warning("NVMe Controller Not Found!");
            return;
        }

        Log::Info("Initializing NVMe Controller...");
        
    }
}